/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#ifdef AIX
#define _LINUX_SOURCE_COMPAT
#include <sys/timer.h>
#undef _LINUX_SOURCE_COMPAT
#endif

#include <com/sun/star/accessibility/TextSegment.hpp>
#include <com/sun/star/accessibility/AccessibleEventId.hpp>
#include <com/sun/star/accessibility/AccessibleStateType.hpp>
#include <com/sun/star/accessibility/AccessibleTableModelChange.hpp>
#include <com/sun/star/accessibility/AccessibleTableModelChangeType.hpp>
#include <com/sun/star/accessibility/XAccessibleEventBroadcaster.hpp>
#include <com/sun/star/accessibility/XAccessibleContext3.hpp>
#include <com/sun/star/lang/IndexOutOfBoundsException.hpp>

#include "atklistener.hxx"
#include "atkwrapper.hxx"
#include <comphelper/sequence.hxx>
#include <vcl/svapp.hxx>

#include <sal/log.hxx>

#define DEBUG_ATK_LISTENER 0

#if DEBUG_ATK_LISTENER
#include <iostream>
#include <sstream>
#endif

using namespace com::sun::star;

AtkListener::AtkListener( AtkObjectWrapper* pWrapper ) : mpWrapper( pWrapper )
{
    if( mpWrapper )
    {
        g_object_ref( mpWrapper );
        updateChildList( mpWrapper->mpContext );
    }
}

AtkListener::~AtkListener()
{
    if( mpWrapper )
        g_object_unref( mpWrapper );
}

/*****************************************************************************/

static AtkStateType mapState( const uno::Any &rAny )
{
    sal_Int16 nState = accessibility::AccessibleStateType::INVALID;
    rAny >>= nState;
    return mapAtkState( nState );
}

/*****************************************************************************/

extern "C" {
    // rhbz#1001768 - down to horrific problems releasing the solar mutex
    // while destroying a Window - which occurs inside these notifications.
    static gboolean
    idle_defunc_state_change( AtkObject *atk_obj )
    {
        SolarMutexGuard aGuard;

        // This is an equivalent to a state change to DEFUNC(T).
        atk_object_notify_state_change( atk_obj, ATK_STATE_DEFUNCT, true );
        if( atk_get_focus_object() == atk_obj )
        {
            SAL_WNODEPRECATED_DECLARATIONS_PUSH
            atk_focus_tracker_notify( nullptr );
            SAL_WNODEPRECATED_DECLARATIONS_POP
        }
        g_object_unref( G_OBJECT( atk_obj ) );
        return false;
    }
}

// XEventListener implementation
void AtkListener::disposing( const lang::EventObject& )
{
    if( !mpWrapper )
        return;

    AtkObject *atk_obj = ATK_OBJECT( mpWrapper );

    // Release all interface references to avoid shutdown problems with
    // global mutex
    atk_object_wrapper_dispose( mpWrapper );

    g_idle_add( reinterpret_cast<GSourceFunc>(idle_defunc_state_change),
                g_object_ref( G_OBJECT( atk_obj ) ) );

    // Release the wrapper object so that it can vanish ..
    g_object_unref( mpWrapper );
    mpWrapper = nullptr;
}

/*****************************************************************************/

static AtkObject *getObjFromAny( const uno::Any &rAny )
{
    uno::Reference< accessibility::XAccessible > xAccessible;
    rAny >>= xAccessible;
    return xAccessible.is() ? atk_object_wrapper_ref( xAccessible ) : nullptr;
}

/*****************************************************************************/

// Updates the child list held to provide the old IndexInParent on children_changed::remove
void AtkListener::updateChildList(
    css::uno::Reference<css::accessibility::XAccessibleContext> const &
        pContext)
{
    m_aChildList.clear();

    uno::Reference< accessibility::XAccessibleStateSet > xStateSet = pContext->getAccessibleStateSet();
    if( !xStateSet.is()
        || xStateSet->contains(accessibility::AccessibleStateType::DEFUNC)
        || xStateSet->contains(accessibility::AccessibleStateType::MANAGES_DESCENDANTS) )
        return;

    css::uno::Reference<css::accessibility::XAccessibleContext3> xContext3(pContext, css::uno::UNO_QUERY);
    if (xContext3.is())
    {
        m_aChildList = comphelper::sequenceToContainer<std::vector<css::uno::Reference< css::accessibility::XAccessible >>>(xContext3->getAccessibleChildren());
    }
    else
    {
        sal_Int32 nChildren = pContext->getAccessibleChildCount();
        m_aChildList.resize(nChildren);
        for(sal_Int32 n = 0; n < nChildren; n++)
        {
            try
            {
                m_aChildList[n] = pContext->getAccessibleChild(n);
            }
            catch (lang::IndexOutOfBoundsException const&)
            {
                sal_Int32 nChildren2 = pContext->getAccessibleChildCount();
                assert(nChildren2 <= n && "consistency?");
                m_aChildList.resize(std::min(nChildren2, n));
                break;
            }
        }
    }
}

/*****************************************************************************/

void AtkListener::handleChildAdded(
    const uno::Reference< accessibility::XAccessibleContext >& rxParent,
    const uno::Reference< accessibility::XAccessible>& rxAccessible)
{
    AtkObject * pChild = rxAccessible.is() ? atk_object_wrapper_ref( rxAccessible ) : nullptr;

    if( pChild )
    {
        updateChildList(rxParent);

        atk_object_wrapper_add_child( mpWrapper, pChild,
            atk_object_get_index_in_parent( pChild ));

        g_object_unref( pChild );
    }
}

/*****************************************************************************/

void AtkListener::handleChildRemoved(
    const uno::Reference< accessibility::XAccessibleContext >& rxParent,
    const uno::Reference< accessibility::XAccessible>& rxChild)
{
    sal_Int32 nIndex = -1;

    // Locate the child in the children list
    size_t n, nmax = m_aChildList.size();
    for( n = 0; n < nmax; ++n )
    {
        if( rxChild == m_aChildList[n] )
        {
            nIndex = n;
            break;
        }
    }

    // FIXME: two problems here:
    // a) we get child-removed events for objects that are no real children
    //    in the accessibility hierarchy or have been removed before due to
    //    some child removing batch.
    // b) spi_atk_bridge_signal_listener ignores the given parameters
    //    for children_changed events and always asks the parent for the
    //    0. child, which breaks somehow on vanishing list boxes.
    // Ignoring "remove" events for objects not in the m_aChildList
    // for now.
    if( nIndex < 0 )
        return;

    uno::Reference<accessibility::XAccessibleEventBroadcaster> xBroadcaster(
        rxChild->getAccessibleContext(), uno::UNO_QUERY);

    if (xBroadcaster.is())
    {
        uno::Reference<accessibility::XAccessibleEventListener> xListener(this);
        xBroadcaster->removeAccessibleEventListener(xListener);
    }

    updateChildList(rxParent);

    AtkObject * pChild = atk_object_wrapper_ref( rxChild, false );
    if( pChild )
    {
        atk_object_wrapper_remove_child( mpWrapper, pChild, nIndex );
        g_object_unref( pChild );
    }
}

/*****************************************************************************/

void AtkListener::handleInvalidateChildren(
    const uno::Reference< accessibility::XAccessibleContext >& rxParent)
{
    // Send notifications for all previous children
    size_t n = m_aChildList.size();
    while( n-- > 0 )
    {
        if( m_aChildList[n].is() )
        {
            AtkObject * pChild = atk_object_wrapper_ref( m_aChildList[n], false );
            if( pChild )
            {
                atk_object_wrapper_remove_child( mpWrapper, pChild, n );
                g_object_unref( pChild );
            }
        }
    }

    updateChildList(rxParent);

    // Send notifications for all new children
    size_t nmax = m_aChildList.size();
    for( n = 0; n < nmax; ++n )
    {
        if( m_aChildList[n].is() )
        {
            AtkObject * pChild = atk_object_wrapper_ref( m_aChildList[n] );

            if( pChild )
            {
                atk_object_wrapper_add_child( mpWrapper, pChild, n );
                g_object_unref( pChild );
            }
        }
    }
}

/*****************************************************************************/

static uno::Reference< accessibility::XAccessibleContext >
getAccessibleContextFromSource( const uno::Reference< uno::XInterface >& rxSource )
{
    uno::Reference< accessibility::XAccessibleContext > xContext(rxSource, uno::UNO_QUERY);
    if( ! xContext.is() )
    {
         g_warning( "ERROR: Event source does not implement XAccessibleContext" );

         // Second try - query for XAccessible, which should give us access to
         // XAccessibleContext.
         uno::Reference< accessibility::XAccessible > xAccessible(rxSource, uno::UNO_QUERY);
         if( xAccessible.is() )
             xContext = xAccessible->getAccessibleContext();
    }

    return xContext;
}

#if DEBUG_ATK_LISTENER

namespace {

void printNotifyEvent( const accessibility::AccessibleEventObject& rEvent )
{
    static std::vector<const char*> aLabels = {
        0,
        "NAME_CHANGED",                                       // 01
        "DESCRIPTION_CHANGED",                                // 02
        "ACTION_CHANGED",                                     // 03
        "STATE_CHANGED",                                      // 04
        "ACTIVE_DESCENDANT_CHANGED",                          // 05
        "BOUNDRECT_CHANGED",                                  // 06
        "CHILD",                                              // 07
        "INVALIDATE_ALL_CHILDREN",                            // 08
        "SELECTION_CHANGED",                                  // 09
        "VISIBLE_DATA_CHANGED",                               // 10
        "VALUE_CHANGED",                                      // 11
        "CONTENT_FLOWS_FROM_RELATION_CHANGED",                // 12
        "CONTENT_FLOWS_TO_RELATION_CHANGED",                  // 13
        "CONTROLLED_BY_RELATION_CHANGED",                     // 14
        "CONTROLLER_FOR_RELATION_CHANGED",                    // 15
        "LABEL_FOR_RELATION_CHANGED",                         // 16
        "LABELED_BY_RELATION_CHANGED",                        // 17
        "MEMBER_OF_RELATION_CHANGED",                         // 18
        "SUB_WINDOW_OF_RELATION_CHANGED",                     // 19
        "CARET_CHANGED",                                      // 20
        "TEXT_SELECTION_CHANGED",                             // 21
        "TEXT_CHANGED",                                       // 22
        "TEXT_ATTRIBUTE_CHANGED",                             // 23
        "HYPERTEXT_CHANGED",                                  // 24
        "TABLE_CAPTION_CHANGED",                              // 25
        "TABLE_COLUMN_DESCRIPTION_CHANGED",                   // 26
        "TABLE_COLUMN_HEADER_CHANGED",                        // 27
        "TABLE_MODEL_CHANGED",                                // 28
        "TABLE_ROW_DESCRIPTION_CHANGED",                      // 29
        "TABLE_ROW_HEADER_CHANGED",                           // 30
        "TABLE_SUMMARY_CHANGED",                              // 31
        "LISTBOX_ENTRY_EXPANDED",                             // 32
        "LISTBOX_ENTRY_COLLAPSED",                            // 33
        "ACTIVE_DESCENDANT_CHANGED_NOFOCUS",                  // 34
        "SELECTION_CHANGED_ADD",                              // 35
        "SELECTION_CHANGED_REMOVE",                           // 36
        "SELECTION_CHANGED_WITHIN",                           // 37
        "PAGE_CHANGED",                                       // 38
        "SECTION_CHANGED",                                    // 39
        "COLUMN_CHANGED",                                     // 40
        "ROLE_CHANGED",                                       // 41
    };

    static std::vector<const char*> aStates = {
        "INVALID",             // 00
        "ACTIVE",              // 01
        "ARMED",               // 02
        "BUSY",                // 03
        "CHECKED",             // 04
        "DEFUNC",              // 05
        "EDITABLE",            // 06
        "ENABLED",             // 07
        "EXPANDABLE",          // 08
        "EXPANDED",            // 09
        "FOCUSABLE",           // 10
        "FOCUSED",             // 11
        "HORIZONTAL",          // 12
        "ICONIFIED",           // 13
        "INDETERMINATE",       // 14
        "MANAGES_DESCENDANTS", // 15
        "MODAL",               // 16
        "MULTI_LINE",          // 17
        "MULTI_SELECTABLE",    // 18
        "OPAQUE",              // 19
        "PRESSED",             // 20
        "RESIZABLE",           // 21
        "SELECTABLE",          // 22
        "SELECTED",            // 23
        "SENSITIVE",           // 24
        "SHOWING",             // 25
        "SINGLE_LINE",         // 26
        "STALE",               // 27
        "TRANSIENT",           // 28
        "VERTICAL",            // 29
        "VISIBLE",             // 30
        "MOVEABLE",            // 31
        "DEFAULT",             // 32
        "OFFSCREEN",           // 33
        "COLLAPSE",            // 34
    };

    auto getOrUnknown = [](const std::vector<const char*>& rCont, size_t nIndex) -> std::string
    {
        return (nIndex < rCont.size()) ? rCont[nIndex] : "<unknown>";
    };

    std::ostringstream os;
    os << "--" << std::endl;
    os << "* event = " << getOrUnknown(aLabels, rEvent.EventId) << std::endl;

    switch (rEvent.EventId)
    {
        case accessibility::AccessibleEventId::STATE_CHANGED:
        {
            sal_Int16 nState;
            if (rEvent.OldValue >>= nState)
                os << "  * old state = " << getOrUnknown(aStates, nState);
            if (rEvent.NewValue >>= nState)
                os << "  * new state = " << getOrUnknown(aStates, nState);

            os << std::endl;
            break;
        }
        default:
            ;
    }

    std::cout << os.str();
}

}

#endif

void AtkListener::notifyEvent( const accessibility::AccessibleEventObject& aEvent )
{
    if( !mpWrapper )
        return;

    AtkObject *atk_obj = ATK_OBJECT( mpWrapper );

    switch( aEvent.EventId )
    {
    // AtkObject signals:
        // Hierarchy signals
        case accessibility::AccessibleEventId::CHILD:
        {
            uno::Reference< accessibility::XAccessibleContext > xParent;
            uno::Reference< accessibility::XAccessible > xChild;

            xParent = getAccessibleContextFromSource(aEvent.Source);
            g_return_if_fail( xParent.is() );

            if( aEvent.OldValue >>= xChild )
                handleChildRemoved(xParent, xChild);

            if( aEvent.NewValue >>= xChild )
                handleChildAdded(xParent, xChild);
            break;
        }

        case accessibility::AccessibleEventId::INVALIDATE_ALL_CHILDREN:
        {
            uno::Reference< accessibility::XAccessibleContext > xParent = getAccessibleContextFromSource(aEvent.Source);
            g_return_if_fail( xParent.is() );

            handleInvalidateChildren(xParent);
            break;
        }

        case accessibility::AccessibleEventId::NAME_CHANGED:
        {
            OUString aName;
            if( aEvent.NewValue >>= aName )
            {
                atk_object_set_name(atk_obj,
                    OUStringToOString(aName, RTL_TEXTENCODING_UTF8).getStr());
            }
            break;
        }

        case accessibility::AccessibleEventId::DESCRIPTION_CHANGED:
        {
            OUString aDescription;
            if( aEvent.NewValue >>= aDescription )
            {
                atk_object_set_description(atk_obj,
                    OUStringToOString(aDescription, RTL_TEXTENCODING_UTF8).getStr());
            }
            break;
        }

        case accessibility::AccessibleEventId::STATE_CHANGED:
        {
            AtkStateType eOldState = mapState( aEvent.OldValue );
            AtkStateType eNewState = mapState( aEvent.NewValue );

            bool bState = eNewState != ATK_STATE_INVALID;
            AtkStateType eRealState = bState ? eNewState : eOldState;

            atk_object_notify_state_change( atk_obj, eRealState, bState );
            break;
        }

        case accessibility::AccessibleEventId::BOUNDRECT_CHANGED:

#ifdef HAS_ATKRECTANGLE
            if( ATK_IS_COMPONENT( atk_obj ) )
            {
                AtkRectangle rect;

                atk_component_get_extents( ATK_COMPONENT( atk_obj ),
                                           &rect.x,
                                           &rect.y,
                                           &rect.width,
                                           &rect.height,
                                           ATK_XY_SCREEN );

                g_signal_emit_by_name( atk_obj, "bounds_changed", &rect );
            }
            else
                g_warning( "bounds_changed event for object not implementing AtkComponent\n");
#endif

            break;

        case accessibility::AccessibleEventId::VISIBLE_DATA_CHANGED:
            g_signal_emit_by_name( atk_obj, "visible-data-changed" );
            break;

        case accessibility::AccessibleEventId::ACTIVE_DESCENDANT_CHANGED:
        {
            AtkObject *pChild = getObjFromAny( aEvent.NewValue );
            if( pChild )
            {
                g_signal_emit_by_name( atk_obj, "active-descendant-changed", pChild );
                g_object_unref( pChild );
            }
            break;
        }

        //ACTIVE_DESCENDANT_CHANGED_NOFOCUS (sic) appears to have been added
        //as a workaround or an aid for the ia2 winaccessibility implementation
        //so ignore it silently without warning here
        case accessibility::AccessibleEventId::ACTIVE_DESCENDANT_CHANGED_NOFOCUS:
            break;

        // #i92103#
        case accessibility::AccessibleEventId::LISTBOX_ENTRY_EXPANDED:
        {
            AtkObject *pChild = getObjFromAny( aEvent.NewValue );
            if( pChild )
            {
                atk_object_notify_state_change( pChild, ATK_STATE_EXPANDED, true );
                g_object_unref( pChild );
            }
            break;
        }

        case accessibility::AccessibleEventId::LISTBOX_ENTRY_COLLAPSED:
        {
            AtkObject *pChild = getObjFromAny( aEvent.NewValue );
            if( pChild )
            {
                atk_object_notify_state_change( pChild, ATK_STATE_EXPANDED, false );
                g_object_unref( pChild );
            }
            break;
        }

        // AtkAction signals ...
        case accessibility::AccessibleEventId::ACTION_CHANGED:
            g_signal_emit_by_name( G_OBJECT( atk_obj ), "property_change::accessible-actions");
            break;

        // AtkText
        case accessibility::AccessibleEventId::CARET_CHANGED:
        {
            sal_Int32 nPos=0;
            aEvent.NewValue >>= nPos;
            g_signal_emit_by_name( atk_obj, "text_caret_moved", nPos );
            break;
        }
        case accessibility::AccessibleEventId::TEXT_CHANGED:
        {
            // cf. comphelper/source/misc/accessibletexthelper.cxx (implInitTextChangedEvent)
            accessibility::TextSegment aDeletedText;
            accessibility::TextSegment aInsertedText;

            if( aEvent.OldValue >>= aDeletedText )
            {
                const OString aDeletedTextUtf8 = OUStringToOString(aDeletedText.SegmentText, RTL_TEXTENCODING_UTF8);
                g_signal_emit_by_name( atk_obj, "text-remove",
                                       static_cast<gint>(aDeletedText.SegmentStart),
                                       static_cast<gint>(aDeletedText.SegmentEnd - aDeletedText.SegmentStart),
                                       g_strdup(aDeletedTextUtf8.getStr()));

            }
            if( aEvent.NewValue >>= aInsertedText )
            {
                const OString aInsertedTextUtf8 = OUStringToOString(aInsertedText.SegmentText, RTL_TEXTENCODING_UTF8);
                g_signal_emit_by_name( atk_obj, "text-insert",
                                       static_cast<gint>(aInsertedText.SegmentStart),
                                       static_cast<gint>(aInsertedText.SegmentEnd - aInsertedText.SegmentStart),
                                       g_strdup(aInsertedTextUtf8.getStr()));
            }
            break;
        }

        case accessibility::AccessibleEventId::TEXT_SELECTION_CHANGED:
        {
            g_signal_emit_by_name( atk_obj, "text-selection-changed" );
            break;
        }

        case accessibility::AccessibleEventId::TEXT_ATTRIBUTE_CHANGED:
            g_signal_emit_by_name( atk_obj, "text-attributes-changed" );
            break;

        // AtkValue
        case accessibility::AccessibleEventId::VALUE_CHANGED:
            g_object_notify( G_OBJECT( atk_obj ), "accessible-value" );
            break;

        case accessibility::AccessibleEventId::CONTENT_FLOWS_FROM_RELATION_CHANGED:
        case accessibility::AccessibleEventId::CONTENT_FLOWS_TO_RELATION_CHANGED:
        case accessibility::AccessibleEventId::CONTROLLED_BY_RELATION_CHANGED:
        case accessibility::AccessibleEventId::CONTROLLER_FOR_RELATION_CHANGED:
        case accessibility::AccessibleEventId::LABEL_FOR_RELATION_CHANGED:
        case accessibility::AccessibleEventId::LABELED_BY_RELATION_CHANGED:
        case accessibility::AccessibleEventId::MEMBER_OF_RELATION_CHANGED:
        case accessibility::AccessibleEventId::SUB_WINDOW_OF_RELATION_CHANGED:
            // FIXME: ask Bill how Atk copes with this little lot ...
            break;

        // AtkTable
        case accessibility::AccessibleEventId::TABLE_MODEL_CHANGED:
        {
            accessibility::AccessibleTableModelChange aChange;
            aEvent.NewValue >>= aChange;

            sal_Int32 nRowsChanged = aChange.LastRow - aChange.FirstRow + 1;
            sal_Int32 nColumnsChanged = aChange.LastColumn - aChange.FirstColumn + 1;

            switch( aChange.Type )
            {
                case accessibility::AccessibleTableModelChangeType::COLUMNS_INSERTED:
                    g_signal_emit_by_name(G_OBJECT(atk_obj), "column-inserted",
                                          aChange.FirstColumn, nColumnsChanged);
                    break;
                case accessibility::AccessibleTableModelChangeType::COLUMNS_REMOVED:
                    g_signal_emit_by_name(G_OBJECT(atk_obj), "column-deleted",
                                          aChange.FirstColumn, nColumnsChanged);
                    break;
                case accessibility::AccessibleTableModelChangeType::ROWS_INSERTED:
                    g_signal_emit_by_name(G_OBJECT(atk_obj), "row-inserted",
                                          aChange.FirstRow, nRowsChanged);
                    break;
                case accessibility::AccessibleTableModelChangeType::ROWS_REMOVED:
                    g_signal_emit_by_name(G_OBJECT(atk_obj), "row-deleted",
                                          aChange.FirstRow, nRowsChanged);
                    break;
                case accessibility::AccessibleTableModelChangeType::UPDATE:
                    // This is not really a model change, is it ?
                    break;
                default:
                    g_warning( "TESTME: unusual table model change %d\n", aChange.Type );
                    break;
            }
            g_signal_emit_by_name( G_OBJECT( atk_obj ), "model-changed" );
            break;
        }

        case accessibility::AccessibleEventId::TABLE_COLUMN_HEADER_CHANGED:
        {
            accessibility::AccessibleTableModelChange aChange;
            aEvent.NewValue >>= aChange;

            AtkPropertyValues values;
            memset(&values,  0, sizeof(AtkPropertyValues));
            g_value_init (&values.new_value, G_TYPE_INT);
            values.property_name = "accessible-table-column-header";

            for (sal_Int32 nChangedColumn = aChange.FirstColumn; nChangedColumn <= aChange.LastColumn; ++nChangedColumn)
            {
                g_value_set_int (&values.new_value, nChangedColumn);
                g_signal_emit_by_name(G_OBJECT(atk_obj), "property_change::accessible-table-column-header", &values, nullptr);
            }
            break;
        }

        case accessibility::AccessibleEventId::TABLE_CAPTION_CHANGED:
            g_signal_emit_by_name( G_OBJECT( atk_obj ), "property_change::accessible-table-caption");
            break;

        case accessibility::AccessibleEventId::TABLE_COLUMN_DESCRIPTION_CHANGED:
            g_signal_emit_by_name( G_OBJECT( atk_obj ), "property_change::accessible-table-column-description");
            break;

        case accessibility::AccessibleEventId::TABLE_ROW_DESCRIPTION_CHANGED:
            g_signal_emit_by_name( G_OBJECT( atk_obj ), "property_change::accessible-table-row-description");
            break;

        case accessibility::AccessibleEventId::TABLE_ROW_HEADER_CHANGED:
            g_signal_emit_by_name( G_OBJECT( atk_obj ), "property_change::accessible-table-row-header");
            break;

        case accessibility::AccessibleEventId::TABLE_SUMMARY_CHANGED:
            g_signal_emit_by_name( G_OBJECT( atk_obj ), "property_change::accessible-table-summary");
            break;

        case accessibility::AccessibleEventId::SELECTION_CHANGED:
        case accessibility::AccessibleEventId::SELECTION_CHANGED_ADD:
        case accessibility::AccessibleEventId::SELECTION_CHANGED_REMOVE:
        case accessibility::AccessibleEventId::SELECTION_CHANGED_WITHIN:
            if (ATK_IS_SELECTION(atk_obj))
                g_signal_emit_by_name(G_OBJECT(atk_obj), "selection_changed");
            else
            {
                // e.g. tdf#122353, when such dialogs become native the problem will go away anyway
                SAL_INFO("vcl.gtk", "selection change from obj which doesn't support XAccessibleSelection");
            }
            break;

        case accessibility::AccessibleEventId::HYPERTEXT_CHANGED:
            g_signal_emit_by_name( G_OBJECT( atk_obj ), "property_change::accessible-hypertext-offset");
            break;

        case accessibility::AccessibleEventId::ROLE_CHANGED:
        {
            uno::Reference< accessibility::XAccessibleContext > xContext = getAccessibleContextFromSource( aEvent.Source );
            atk_object_wrapper_set_role( mpWrapper, xContext->getAccessibleRole() );
            break;
        }

        case accessibility::AccessibleEventId::PAGE_CHANGED:
        {
            /* // If we implemented AtkDocument then I imagine this is what this
               // handler should look like
               sal_Int32 nPos=0;
               aEvent.NewValue >>= nPos;
               g_signal_emit_by_name( G_OBJECT( atk_obj ), "page_changed", nPos );
            */
            break;
        }

        default:
            SAL_WARN("vcl.gtk", "Unknown event notification: " << aEvent.EventId);
            break;
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
