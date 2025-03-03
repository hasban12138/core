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


#pragma once

#include <vcl/accessibletable.hxx>
#include <rtl/ustring.hxx>
#include <rtl/ref.hxx>
#include <tools/gen.hxx>
#include <cppuhelper/compbase4.hxx>
#include <cppuhelper/implbase1.hxx>
#include <cppuhelper/basemutex.hxx>
#include <unotools/accessiblestatesethelper.hxx>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/accessibility/XAccessible.hpp>
#include <com/sun/star/accessibility/XAccessibleContext.hpp>
#include <com/sun/star/accessibility/XAccessibleComponent.hpp>
#include <com/sun/star/accessibility/XAccessibleEventBroadcaster.hpp>
#include <comphelper/accessibleeventnotifier.hxx>
#include <comphelper/uno3.hxx>


namespace vcl { class Window; }

namespace utl {
    class AccessibleStateSetHelper;
}


namespace accessibility {

typedef ::cppu::WeakAggComponentImplHelper4<
            css::accessibility::XAccessibleContext,
            css::accessibility::XAccessibleComponent,
            css::accessibility::XAccessibleEventBroadcaster,
            css::lang::XServiceInfo >
        AccessibleGridControlImplHelper;

/** The GridControl accessible objects inherit from this base class. It
    implements basic functionality for various Accessibility interfaces and
    the event broadcaster and contains the osl::Mutex. */
class AccessibleGridControlBase :
    public ::cppu::BaseMutex,
    public AccessibleGridControlImplHelper
{
public:
    /** Constructor.
        @param rxParent  XAccessible interface of the parent object.
        @param rTable    The Table control.
        @param eObjType  Type of accessible table control. */
    AccessibleGridControlBase(
        const css::uno::Reference< css::accessibility::XAccessible >& rxParent,
        ::vcl::table::IAccessibleTable& rTable,
        ::vcl::table::AccessibleTableControlObjType  eObjType );

protected:
    virtual ~AccessibleGridControlBase() override;

    /** Commits DeFunc event to listeners and cleans up members. */
    virtual void SAL_CALL disposing() override;

public:
    // XAccessibleContext

    /** @return  A reference to the parent accessible object. */
    virtual css::uno::Reference< css::accessibility::XAccessible > SAL_CALL
    getAccessibleParent() override;

    /** @return  The index of this object among the parent's children. */
    virtual sal_Int32 SAL_CALL getAccessibleIndexInParent() override;

    /** @return
            The description of this object.
    */
    virtual OUString SAL_CALL getAccessibleDescription() override;

    /** @return
            The name of this object.
    */
    virtual OUString SAL_CALL getAccessibleName() override;

    /** @return
            The relation set (the GridControl does not have one).
    */
    virtual css::uno::Reference< css::accessibility::XAccessibleRelationSet > SAL_CALL
        getAccessibleRelationSet() override;

    /** @return  The set of current states. */
    virtual css::uno::Reference< css::accessibility::XAccessibleStateSet > SAL_CALL
        getAccessibleStateSet() override;

    /** @return  The parent's locale. */
    virtual css::lang::Locale SAL_CALL getLocale() override;

    /** @return
            The role of this object. Panel, ROWHEADER, COLUMNHEADER, TABLE, TABLE_CELL are supported.
    */
    virtual sal_Int16 SAL_CALL getAccessibleRole() override;

    /*  Derived classes have to implement:
        -   getAccessibleChildCount,
        -   getAccessibleChild,
        -   getAccessibleRole.
        Derived classes may overwrite getAccessibleIndexInParent to increase
        performance. */

    // XAccessibleComponent

    /** @return
        TRUE, if the point lies within the bounding box of this object. */
    virtual sal_Bool SAL_CALL containsPoint( const css::awt::Point& rPoint ) override;

    /** @return  The bounding box of this object. */
    virtual css::awt::Rectangle SAL_CALL getBounds() override;

    /** @return
        The upper left corner of the bounding box relative to the parent. */
    virtual css::awt::Point SAL_CALL getLocation() override;

    /** @return
        The upper left corner of the bounding box in screen coordinates. */
    virtual css::awt::Point SAL_CALL getLocationOnScreen() override;

    /** @return  The size of the bounding box. */
    virtual css::awt::Size SAL_CALL getSize() override;

    virtual sal_Int32 SAL_CALL getForeground(  ) override;
    virtual sal_Int32 SAL_CALL getBackground(  ) override;


    /*  Derived classes have to implement:
        -   getAccessibleAt,
        -   grabFocus. */

    /** @return
            The accessible child rendered under the given point.
    */
    virtual css::uno::Reference< css::accessibility::XAccessible > SAL_CALL
    getAccessibleAtPoint( const css::awt::Point& rPoint ) override;

    // XAccessibleEventBroadcaster

    /** Adds a new event listener */
    virtual void SAL_CALL addAccessibleEventListener(
            const css::uno::Reference< css::accessibility::XAccessibleEventListener>& rxListener ) override;

    /** Removes an event listener. */
    virtual void SAL_CALL removeAccessibleEventListener(
            const css::uno::Reference< css::accessibility::XAccessibleEventListener>& rxListener ) override;

    // XTypeProvider

    /** @return  a unique implementation ID. */
    virtual css::uno::Sequence< sal_Int8 > SAL_CALL getImplementationId() override;

    // XServiceInfo

    /** @return  Whether the specified service is supported by this class. */
    virtual sal_Bool SAL_CALL supportsService( const OUString& rServiceName ) override;

    /** @return  a list of all supported services. */
    virtual css::uno::Sequence< OUString > SAL_CALL
    getSupportedServiceNames() override;

    /*  Derived classes have to implement:
        -   getImplementationName. */

    // helper methods

    /** @return  The GridControl object type. */
    inline ::vcl::table::AccessibleTableControlObjType getType() const;

    /** Commits an event to all listeners. */
    virtual void commitEvent(sal_Int16 nEventId, const css::uno::Any& rNewValue,
                             const css::uno::Any& rOldValue);
    /** @return  TRUE, if the object is not disposed or disposing. */
    bool isAlive() const;

protected:
    // internal virtual methods

    /** Determines whether the Grid control is really showing inside of
        its parent accessible window. Derived classes may implement different
        behaviour.
        @attention  This method requires locked mutex's and a living object.
        @return  TRUE, if the object is really showing. */
    bool implIsShowing();

    /** Derived classes return the bounding box relative to the parent window.
        @attention  This method requires locked mutex's and a living object.
        @return  The bounding box (VCL rect.) relative to the parent window. */
    virtual tools::Rectangle implGetBoundingBox() = 0;
    ///** Derived classes return the bounding box in screen coordinates.
    //    @attention  This method requires locked mutex's and a living object.
    //    @return  The bounding box (VCL rect.) in screen coordinates. */
    virtual tools::Rectangle implGetBoundingBoxOnScreen() = 0;

    /** Creates a new AccessibleStateSetHelper and fills it with states of the
        current object. This method calls FillStateSet at the GridControl which
        fills it with more states depending on the object type. Derived classes
        may overwrite this method and add more states.
        @attention  This method requires locked mutex's.
        @return  A filled AccessibleStateSetHelper. */
    virtual rtl::Reference<::utl::AccessibleStateSetHelper> implCreateStateSetHelper();

    // internal helper methods

    /** @throws <type>DisposedException</type>  If the object is not alive. */
    void ensureIsAlive() const;

    /** Locks all mutex's and calculates the bounding box relative to the
        parent window.
        @return  The bounding box (VCL rect.) relative to the parent object.
        @throws css::lang::DisposedException
    */
    tools::Rectangle getBoundingBox();
    ///** Locks all mutex's and calculates the bounding box in screen
    //    coordinates.
    //    @return  The bounding box (VCL rect.) in screen coordinates. */
    /// @throws css::lang::DisposedException
    tools::Rectangle getBoundingBoxOnScreen();

    ::comphelper::AccessibleEventNotifier::TClientId getClientId() const { return m_aClientId; }
    void setClientId(::comphelper::AccessibleEventNotifier::TClientId _aNewClientId) { m_aClientId = _aNewClientId; }

protected:
    // members

    /** The parent accessible object. */
    css::uno::Reference< css::accessibility::XAccessible > m_xParent;
    /** The SVT Table control. */
    ::vcl::table::IAccessibleTable& m_aTable;
    /** The type of this object (for names, descriptions, state sets, ...). */
    ::vcl::table::AccessibleTableControlObjType m_eObjType;

private:
    ::comphelper::AccessibleEventNotifier::TClientId    m_aClientId;
};


// a version of AccessibleGridControlBase which implements not only the XAccessibleContext,
// but also the XAccessible

typedef ::cppu::ImplHelper1 <   css::accessibility::XAccessible
                            >   GridControlAccessibleElement_Base;

class GridControlAccessibleElement
            :public AccessibleGridControlBase
            ,public GridControlAccessibleElement_Base
{
protected:
    /** Constructor sets specified name and description.

        @param rxParent    XAccessible interface of the parent object.
        @param rTable      The Table control.
        @param eObjType    Type of table control
    */
    GridControlAccessibleElement(
        const css::uno::Reference< css::accessibility::XAccessible >& rxParent,
        ::vcl::table::IAccessibleTable& rTable,
        ::vcl::table::AccessibleTableControlObjType  eObjType );

public:
    // XInterface
    DECLARE_XINTERFACE( )
    // XTypeProvider
    DECLARE_XTYPEPROVIDER( )

protected:
    virtual ~GridControlAccessibleElement() override;

protected:
    // XAccessible

    /** @return  The XAccessibleContext interface of this object. */
    virtual css::uno::Reference< css::accessibility::XAccessibleContext > SAL_CALL
    getAccessibleContext() override;

private:
    GridControlAccessibleElement( const GridControlAccessibleElement& ) = delete;
    GridControlAccessibleElement& operator=( const GridControlAccessibleElement& ) = delete;
};

// inlines

inline ::vcl::table::AccessibleTableControlObjType AccessibleGridControlBase::getType() const
{
    return m_eObjType;
}


} // namespace accessibility



/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
