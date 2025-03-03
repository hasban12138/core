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

#include <comphelper/anytostring.hxx>
#include <comphelper/propertyvalue.hxx>
#include <comphelper/sequenceashashmap.hxx>
#include <o3tl/string_view.hxx>
#include <sal/log.hxx>
#include <tools/multisel.hxx>
#include <tools/diagnose_ex.h>

#include <com/sun/star/container/XNamed.hpp>
#include <com/sun/star/drawing/XMasterPagesSupplier.hpp>
#include <com/sun/star/drawing/XDrawPages.hpp>
#include <com/sun/star/drawing/XDrawPagesSupplier.hpp>
#include <com/sun/star/drawing/XMasterPageTarget.hpp>
#include <com/sun/star/frame/XModel.hpp>
#include <com/sun/star/io/XInputStream.hpp>
#include <com/sun/star/text/XTextField.hpp>
#include <com/sun/star/xml/dom/XDocument.hpp>
#include <com/sun/star/xml/sax/XFastSAXSerializable.hpp>
#include <com/sun/star/presentation/XPresentationPage.hpp>
#include <com/sun/star/task/XStatusIndicator.hpp>
#include <com/sun/star/presentation/XCustomPresentationSupplier.hpp>
#include <com/sun/star/container/XIndexContainer.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/lang/XSingleServiceFactory.hpp>
#include <com/sun/star/container/XNameContainer.hpp>

#include <oox/drawingml/theme.hxx>
#include <oox/drawingml/drawingmltypes.hxx>
#include <oox/drawingml/themefragmenthandler.hxx>
#include <drawingml/textliststylecontext.hxx>
#include <oox/helper/attributelist.hxx>
#include <oox/ole/olestorage.hxx>
#include <oox/ole/vbaproject.hxx>
#include <oox/ppt/pptshape.hxx>
#include <oox/ppt/presentationfragmenthandler.hxx>
#include <oox/ppt/slidefragmenthandler.hxx>
#include <oox/ppt/layoutfragmenthandler.hxx>
#include <oox/ppt/pptimport.hxx>
#include <oox/token/namespaces.hxx>
#include <oox/token/tokens.hxx>

#include <com/sun/star/office/XAnnotation.hpp>
#include <com/sun/star/office/XAnnotationAccess.hpp>

using namespace ::com::sun::star;
using namespace ::oox::core;
using namespace ::oox::drawingml;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::drawing;
using namespace ::com::sun::star::presentation;
using namespace ::com::sun::star::xml::sax;

namespace oox::ppt {

static std::map<PredefinedClrSchemeId, sal_Int32> PredefinedClrTokens =
{
    //{ dk1, XML_dk1 },
    //{ lt1, XML_lt1 },
    { dk2, XML_dk2 },
    { lt2, XML_lt2 },
    { accent1, XML_accent1 },
    { accent2, XML_accent2 },
    { accent3, XML_accent3 },
    { accent4, XML_accent4 },
    { accent5, XML_accent5 },
    { accent6, XML_accent6 },
    { hlink, XML_hlink },
    { folHlink, XML_folHlink }
};

PresentationFragmentHandler::PresentationFragmentHandler(XmlFilterBase& rFilter, const OUString& rFragmentPath)
    : FragmentHandler2( rFilter, rFragmentPath )
    , mpTextListStyle( std::make_shared<TextListStyle>() )
    , mbCommentAuthorsRead(false)
{
    TextParagraphPropertiesArray& rParagraphDefaultsVector( mpTextListStyle->getListStyle() );
    for (auto & elem : rParagraphDefaultsVector)
    {
        // ppt is having zero bottom margin per default, whereas OOo is 0,5cm,
        // so this attribute needs to be set always
        elem.getParaBottomMargin() = TextSpacing( 0 );
    }
}

PresentationFragmentHandler::~PresentationFragmentHandler() noexcept
{
}

static void ResolveTextFields( XmlFilterBase const & rFilter )
{
    const oox::core::TextFieldStack& rTextFields = rFilter.getTextFieldStack();
    if ( rTextFields.empty() )
        return;

    const Reference< frame::XModel >& xModel( rFilter.getModel() );
    for (auto const& textField : rTextFields)
    {
        static const OUStringLiteral sURL = u"URL";
        Reference< drawing::XDrawPagesSupplier > xDPS( xModel, uno::UNO_QUERY_THROW );
        Reference< drawing::XDrawPages > xDrawPages( xDPS->getDrawPages(), uno::UNO_SET_THROW );

        const oox::core::TextField& rTextField( textField );
        Reference< XPropertySet > xPropSet( rTextField.xTextField, UNO_QUERY );
        Reference< XPropertySetInfo > xPropSetInfo( xPropSet->getPropertySetInfo() );
        if ( xPropSetInfo->hasPropertyByName( sURL ) )
        {
            OUString aURL;
            if ( xPropSet->getPropertyValue( sURL ) >>= aURL )
            {
                static const OUStringLiteral sSlide = u"#Slide ";
                static const OUStringLiteral sNotes = u"#Notes ";
                bool bNotes = false;
                sal_Int32 nPageNumber = 0;
                if ( aURL.match( sSlide ) )
                    nPageNumber = o3tl::toInt32(aURL.subView( sSlide.getLength() ));
                else if ( aURL.match( sNotes ) )
                {
                    nPageNumber = o3tl::toInt32(aURL.subView( sNotes.getLength() ));
                    bNotes = true;
                }
                if ( nPageNumber )
                {
                    try
                    {
                        Reference< XDrawPage > xDrawPage;
                        xDrawPages->getByIndex( nPageNumber - 1 ) >>= xDrawPage;
                        if ( bNotes )
                        {
                            Reference< css::presentation::XPresentationPage > xPresentationPage( xDrawPage, UNO_QUERY_THROW );
                            xDrawPage = xPresentationPage->getNotesPage();
                        }
                        Reference< container::XNamed > xNamed( xDrawPage, UNO_QUERY_THROW );
                        aURL = "#" + xNamed->getName();
                        xPropSet->setPropertyValue( sURL, Any( aURL ) );
                        Reference< text::XTextContent > xContent( rTextField.xTextField);
                        Reference< text::XTextRange > xTextRange = rTextField.xTextCursor;
                        rTextField.xText->insertTextContent( xTextRange, xContent, true );
                    }
                    catch( uno::Exception& )
                    {
                    }
                }
            }
        }
    }
}

void PresentationFragmentHandler::importCustomSlideShow(std::vector<CustomShow>& rCustomShowList)
{
    PowerPointImport& rFilter = dynamic_cast<PowerPointImport&>(getFilter());
    Reference<frame::XModel> xModel(rFilter.getModel());
    Reference<XDrawPagesSupplier> xDrawPagesSupplier(xModel, UNO_QUERY_THROW);
    Reference<XDrawPages> xDrawPages(xDrawPagesSupplier->getDrawPages(), UNO_SET_THROW);

    Reference<css::lang::XSingleServiceFactory> mxShowFactory;
    Reference<css::container::XNameContainer> mxShows;
    Reference<XCustomPresentationSupplier> xShowsSupplier(xModel, UNO_QUERY);
    if (xShowsSupplier.is())
    {
        mxShows = xShowsSupplier->getCustomPresentations();
        mxShowFactory.set(mxShows, UNO_QUERY);
    }

    for (size_t i = 0; i < rCustomShowList.size(); ++i)
    {
        Reference<com::sun::star::container::XIndexContainer> xShow(mxShowFactory->createInstance(),
                                                                    UNO_QUERY);
        if (xShow.is())
        {
            static const OUStringLiteral sSlide = u"slides/slide";
            for (size_t j = 0; j < rCustomShowList[i].maSldLst.size(); ++j)
            {
                OUString sCustomSlide = rCustomShowList[i].maSldLst[j];
                sal_Int32 nPageNumber = 0;
                if (sCustomSlide.match(sSlide))
                    nPageNumber = o3tl::toInt32(sCustomSlide.subView(sSlide.getLength()));

                Reference<XDrawPage> xPage;
                xDrawPages->getByIndex(nPageNumber - 1) >>= xPage;
                if (xPage.is())
                    xShow->insertByIndex(xShow->getCount(), Any(xPage));
            }

            Any aAny;
            aAny <<= xShow;
            mxShows->insertByName(rCustomShowList[i].maCustomShowName, aAny);
        }
    }
}

void PresentationFragmentHandler::saveThemeToGrabBag(const oox::drawingml::ThemePtr& pThemePtr,
                                                     sal_Int32 nThemeIdx)
{
    if (!pThemePtr)
        return;

    try
    {
        uno::Reference<beans::XPropertySet> xDocProps(getFilter().getModel(), uno::UNO_QUERY);
        if (xDocProps.is())
        {
            uno::Reference<beans::XPropertySetInfo> xPropsInfo = xDocProps->getPropertySetInfo();

            static const OUStringLiteral aGrabBagPropName = u"InteropGrabBag";
            if (xPropsInfo.is() && xPropsInfo->hasPropertyByName(aGrabBagPropName))
            {
                // get existing grab bag
                comphelper::SequenceAsHashMap aGrabBag(xDocProps->getPropertyValue(aGrabBagPropName));

                comphelper::SequenceAsHashMap aThemesHashMap;

                // create current theme
                uno::Sequence<beans::PropertyValue> aCurrentTheme(PredefinedClrSchemeId::Count);
                auto pCurrentTheme = aCurrentTheme.getArray();

                ClrScheme rClrScheme = pThemePtr->getClrScheme();
                for (int nId = PredefinedClrSchemeId::dk2; nId != PredefinedClrSchemeId::Count; nId++)
                {
                    sal_uInt32 nToken = PredefinedClrTokens[static_cast<PredefinedClrSchemeId>(nId)];
                    const OUString& sName = PredefinedClrNames[static_cast<PredefinedClrSchemeId>(nId)];
                    ::Color nColor;

                    rClrScheme.getColor(nToken, nColor);
                    const uno::Any& rColor = uno::makeAny(nColor);

                    pCurrentTheme[nId].Name = sName;
                    pCurrentTheme[nId].Value = rColor;
                }


                uno::Sequence<beans::PropertyValue> aTheme{
                    // add new theme to the sequence
                    // Export code uses the master slide's index to find the right theme
                    // so use the same index in the grabbag.
                    comphelper::makePropertyValue(
                        "ppt/theme/theme" + OUString::number(nThemeIdx) + ".xml", aCurrentTheme),
                    // store DOM fragment for SmartArt re-generation
                    comphelper::makePropertyValue("OOXTheme", pThemePtr->getFragment())
                };

                aThemesHashMap << aTheme;

                // put the new items
                aGrabBag.update(aThemesHashMap);

                // put it back to the document
                xDocProps->setPropertyValue(aGrabBagPropName, uno::Any(aGrabBag.getAsConstPropertyValueList()));
            }
        }
    }
    catch (const uno::Exception&)
    {
        SAL_WARN("oox", "oox::ppt::PresentationFragmentHandler::saveThemeToGrabBag, Failed to save grab bag");
    }
}

void PresentationFragmentHandler::importSlide(sal_uInt32 nSlide, bool bFirstPage, bool bImportNotesPage)
{
    PowerPointImport& rFilter = dynamic_cast< PowerPointImport& >( getFilter() );

    Reference< frame::XModel > xModel( rFilter.getModel() );
    Reference< drawing::XDrawPage > xSlide;

    // importing slide pages and its corresponding notes page
    Reference< drawing::XDrawPagesSupplier > xDPS( xModel, uno::UNO_QUERY_THROW );
    Reference< drawing::XDrawPages > xDrawPages( xDPS->getDrawPages(), uno::UNO_SET_THROW );

    try {

        if( bFirstPage )
            xDrawPages->getByIndex( 0 ) >>= xSlide;
        else
            xSlide = xDrawPages->insertNewByIndex( xDrawPages->getCount() );

        OUString aSlideFragmentPath = getFragmentPathFromRelId( maSlidesVector[ nSlide ] );
        if( !aSlideFragmentPath.isEmpty() )
        {
            SlidePersistPtr pMasterPersistPtr;
            SlidePersistPtr pSlidePersistPtr = std::make_shared<SlidePersist>( rFilter, false, false, xSlide,
                                std::make_shared<PPTShape>( Slide, "com.sun.star.drawing.GroupShape" ), mpTextListStyle );

            FragmentHandlerRef xSlideFragmentHandler( new SlideFragmentHandler( rFilter, aSlideFragmentPath, pSlidePersistPtr, Slide ) );

            // importing the corresponding masterpage/layout
            OUString aLayoutFragmentPath = xSlideFragmentHandler->getFragmentPathFromFirstTypeFromOfficeDoc( u"slideLayout" );
            OUString aCommentFragmentPath = xSlideFragmentHandler->getFragmentPathFromFirstTypeFromOfficeDoc( u"comments" );
            if ( !aLayoutFragmentPath.isEmpty() )
            {
                // importing layout
                RelationsRef xLayoutRelations = rFilter.importRelations( aLayoutFragmentPath );
                OUString aMasterFragmentPath = xLayoutRelations->getFragmentPathFromFirstTypeFromOfficeDoc( u"slideMaster" );
                if( !aMasterFragmentPath.isEmpty() )
                {
                    // check if the corresponding masterpage+layout has already been imported
                    std::vector< SlidePersistPtr >& rMasterPages( rFilter.getMasterPages() );
                    for (auto const& masterPage : rMasterPages)
                    {
                        if ( ( masterPage->getPath() == aMasterFragmentPath ) && ( masterPage->getLayoutPath() == aLayoutFragmentPath ) )
                        {
                            pMasterPersistPtr = masterPage;
                            break;
                        }
                    }

                    if ( !pMasterPersistPtr )
                    {   // masterpersist not found, we have to load it
                        Reference< drawing::XDrawPage > xMasterPage;
                        Reference< drawing::XMasterPagesSupplier > xMPS( xModel, uno::UNO_QUERY_THROW );
                        Reference< drawing::XDrawPages > xMasterPages( xMPS->getMasterPages(), uno::UNO_SET_THROW );

                        sal_Int32 nIndex;
                        if( rFilter.getMasterPages().empty() )
                        {
                            nIndex = 0;
                            xMasterPages->getByIndex( nIndex ) >>= xMasterPage;
                        }
                        else
                        {
                            nIndex = xMasterPages->getCount();
                            xMasterPage = xMasterPages->insertNewByIndex( nIndex );
                        }

                        pMasterPersistPtr = std::make_shared<SlidePersist>( rFilter, true, false, xMasterPage,
                            std::make_shared<PPTShape>( Master, "com.sun.star.drawing.GroupShape" ), mpTextListStyle );
                        pMasterPersistPtr->setLayoutPath( aLayoutFragmentPath );
                        rFilter.getMasterPages().push_back( pMasterPersistPtr );
                        rFilter.setActualSlidePersist( pMasterPersistPtr );
                        FragmentHandlerRef xMasterFragmentHandler( new SlideFragmentHandler( rFilter, aMasterFragmentPath, pMasterPersistPtr, Master ) );

                        // set the correct theme
                        OUString aThemeFragmentPath = xMasterFragmentHandler->getFragmentPathFromFirstTypeFromOfficeDoc( u"theme" );
                        if( !aThemeFragmentPath.isEmpty() )
                        {
                            std::map< OUString, oox::drawingml::ThemePtr >& rThemes( rFilter.getThemes() );
                            std::map< OUString, oox::drawingml::ThemePtr >::iterator aIter2( rThemes.find( aThemeFragmentPath ) );
                            if( aIter2 == rThemes.end() )
                            {
                                oox::drawingml::ThemePtr pThemePtr = std::make_shared<oox::drawingml::Theme>();
                                pMasterPersistPtr->setTheme( pThemePtr );
                                Reference<xml::dom::XDocument> xDoc=
                                    rFilter.importFragment(aThemeFragmentPath);

                                rFilter.importFragment(
                                    new ThemeFragmentHandler(
                                        rFilter, aThemeFragmentPath, *pThemePtr ),
                                    Reference<xml::sax::XFastSAXSerializable>(
                                        xDoc,
                                        UNO_QUERY_THROW));
                                rThemes[ aThemeFragmentPath ] = pThemePtr;
                                pThemePtr->setFragment(xDoc);
                                saveThemeToGrabBag(pThemePtr, nIndex + 1);
                            }
                            else
                            {
                                pMasterPersistPtr->setTheme( (*aIter2).second );
                            }
                        }
                        importSlide( xMasterFragmentHandler, pMasterPersistPtr );
                        rFilter.importFragment( new LayoutFragmentHandler( rFilter, aLayoutFragmentPath, pMasterPersistPtr ) );
                        pMasterPersistPtr->createBackground( rFilter );
                        pMasterPersistPtr->createXShapes( rFilter );

                        oox::drawingml::ThemePtr pTheme = pMasterPersistPtr->getTheme();
                        if (pTheme)
                        {
                            pTheme->addTheme(pMasterPersistPtr->getPage());
                        }
                    }
                }
            }

            // importing slide page
            if (pMasterPersistPtr) {
                pSlidePersistPtr->setMasterPersist( pMasterPersistPtr );
                pSlidePersistPtr->setTheme( pMasterPersistPtr->getTheme() );
                Reference< drawing::XMasterPageTarget > xMasterPageTarget( pSlidePersistPtr->getPage(), UNO_QUERY );
                if( xMasterPageTarget.is() )
                    xMasterPageTarget->setMasterPage( pMasterPersistPtr->getPage() );
            }
            rFilter.getDrawPages().push_back( pSlidePersistPtr );
            rFilter.setActualSlidePersist( pSlidePersistPtr );
            importSlide( xSlideFragmentHandler, pSlidePersistPtr );
            pSlidePersistPtr->createBackground( rFilter );
            pSlidePersistPtr->createXShapes( rFilter );

            if(bImportNotesPage) {

                // now importing the notes page
                OUString aNotesFragmentPath = xSlideFragmentHandler->getFragmentPathFromFirstTypeFromOfficeDoc( u"notesSlide" );
                if( !aNotesFragmentPath.isEmpty() )
                {
                    Reference< XPresentationPage > xPresentationPage( xSlide, UNO_QUERY );
                    if ( xPresentationPage.is() )
                    {
                        Reference< XDrawPage > xNotesPage( xPresentationPage->getNotesPage() );
                        if ( xNotesPage.is() )
                        {
                            SlidePersistPtr pNotesPersistPtr = std::make_shared<SlidePersist>( rFilter, false, true, xNotesPage,
                                std::make_shared<PPTShape>( Slide, "com.sun.star.drawing.GroupShape" ), mpTextListStyle );
                            FragmentHandlerRef xNotesFragmentHandler( new SlideFragmentHandler( getFilter(), aNotesFragmentPath, pNotesPersistPtr, Slide ) );
                            rFilter.getNotesPages().push_back( pNotesPersistPtr );
                            rFilter.setActualSlidePersist( pNotesPersistPtr );
                            importSlide( xNotesFragmentHandler, pNotesPersistPtr );
                            pNotesPersistPtr->createBackground( rFilter );
                            pNotesPersistPtr->createXShapes( rFilter );
                        }
                    }
                }
            }

            if( !mbCommentAuthorsRead && !aCommentFragmentPath.isEmpty() )
            {
                // Comments are present and commentAuthors.xml has still not been read
                mbCommentAuthorsRead = true;
                Reference< XPresentationPage > xPresentationPage( xSlide, UNO_QUERY );
                Reference< XDrawPage > xCommentAuthorsPage( xPresentationPage->getNotesPage() );
                SlidePersistPtr pCommentAuthorsPersistPtr =
                    std::make_shared<SlidePersist>( rFilter, false, true, xCommentAuthorsPage,
                                      std::make_shared<PPTShape>(
                                              Slide, "com.sun.star.drawing.GroupShape" ),
                                      mpTextListStyle );
                FragmentHandlerRef xCommentAuthorsFragmentHandler(
                    new SlideFragmentHandler( getFilter(),
                                              "ppt/commentAuthors.xml",
                                              pCommentAuthorsPersistPtr,
                                              Slide ) );

                getFilter().importFragment( xCommentAuthorsFragmentHandler );
                maAuthorList.setValues( pCommentAuthorsPersistPtr->getCommentAuthors() );
            }
            if( !aCommentFragmentPath.isEmpty() )
            {
                Reference< XPresentationPage > xPresentationPage( xSlide, UNO_QUERY );
                Reference< XDrawPage > xCommentsPage( xPresentationPage->getNotesPage() );
                SlidePersistPtr pCommentsPersistPtr =
                    std::make_shared<SlidePersist>(
                        rFilter, false, true, xCommentsPage,
                        std::make_shared<PPTShape>(
                                Slide, "com.sun.star.drawing.GroupShape" ),
                        mpTextListStyle );

                FragmentHandlerRef xCommentsFragmentHandler(
                    new SlideFragmentHandler(
                        getFilter(),
                        aCommentFragmentPath,
                        pCommentsPersistPtr,
                        Slide ) );
                pCommentsPersistPtr->getCommentsList().cmLst.clear();
                getFilter().importFragment( xCommentsFragmentHandler );

                if (!pCommentsPersistPtr->getCommentsList().cmLst.empty())
                {
                    //set comment chars for last comment on slide
                    SlideFragmentHandler* comment_handler =
                        dynamic_cast<SlideFragmentHandler*>(xCommentsFragmentHandler.get());
                    assert(comment_handler);
                    // some comments have no text -> set empty string as text to avoid
                    // crash (back() on empty vector is undefined) and losing other
                    // comment data that might be there (author, position, timestamp etc.)
                    pCommentsPersistPtr->getCommentsList().cmLst.back().setText(
                            comment_handler->getCharVector().empty() ? "" :
                            comment_handler->getCharVector().back() );
                }
                pCommentsPersistPtr->getCommentAuthors().setValues(maAuthorList);

                //insert all comments from commentsList
                for(int i=0; i<pCommentsPersistPtr->getCommentsList().getSize(); i++)
                {
                    try {
                        Comment aComment = pCommentsPersistPtr->getCommentsList().getCommentAtIndex(i);
                        uno::Reference< office::XAnnotationAccess > xAnnotationAccess( xSlide, UNO_QUERY_THROW );
                        uno::Reference< office::XAnnotation > xAnnotation( xAnnotationAccess->createAndInsertAnnotation() );
                        int nPosX = aComment.getIntX();
                        int nPosY = aComment.getIntY();
                        xAnnotation->setPosition(
                            geometry::RealPoint2D(
                                ::oox::drawingml::convertEmuToHmm( nPosX ) * 15.87,
                                ::oox::drawingml::convertEmuToHmm( nPosY ) * 15.87 ) );
                        xAnnotation->setAuthor( aComment.getAuthor(maAuthorList) );
                        xAnnotation->setDateTime( aComment.getDateTime() );
                        uno::Reference< text::XText > xText( xAnnotation->getTextRange() );
                        xText->setString( aComment.get_text());
                    } catch( css::lang::IllegalArgumentException& ) {}
                }
            }
        }
    }
    catch( uno::Exception& )
    {
        TOOLS_WARN_EXCEPTION( "oox", "oox::ppt::PresentationFragmentHandler::EndDocument()" );
    }
}

void PresentationFragmentHandler::finalizeImport()
{
    PowerPointImport& rFilter = dynamic_cast< PowerPointImport& >( getFilter() );

    sal_Int32 nPageCount = maSlidesVector.size();

    // we will take the FilterData property "PageRange" if available, otherwise full range is used
    comphelper::SequenceAsHashMap& rFilterData = rFilter.getFilterData();

    // writing back the original PageCount of this document, it can be accessed from the XModel
    // via getArgs after the import.
    rFilterData["OriginalPageCount"] <<= nPageCount;
    bool bImportNotesPages = rFilterData.getUnpackedValueOrDefault("ImportNotesPages", true);
    OUString aPageRange = rFilterData.getUnpackedValueOrDefault("PageRange", OUString());

    if( !aPageRange.getLength() )
    {
        aPageRange = "1-" + OUString::number( nPageCount );
    }

    StringRangeEnumerator aRangeEnumerator( aPageRange, 0, nPageCount - 1 );
    if (aRangeEnumerator.size())
    {
        // todo: localized progress bar text
        const Reference< task::XStatusIndicator >& rxStatusIndicator( getFilter().getStatusIndicator() );
        if ( rxStatusIndicator.is() )
            rxStatusIndicator->start( OUString(), 10000 );

        try
        {
            int nPagesImported = 0;
            for (sal_Int32 elem : aRangeEnumerator)
            {
                if ( rxStatusIndicator.is() )
                    rxStatusIndicator->setValue((nPagesImported * 10000) / aRangeEnumerator.size());

                importSlide(elem, !nPagesImported, bImportNotesPages);
                nPagesImported++;
            }
            ResolveTextFields( rFilter );
            if (!maCustomShowList.empty())
                importCustomSlideShow(maCustomShowList);
        }
        catch( uno::Exception& )
        {
            TOOLS_WARN_EXCEPTION( "oox", "oox::ppt::PresentationFragmentHandler::finalizeImport()" );
        }
        // todo error handling;
        if ( rxStatusIndicator.is() )
            rxStatusIndicator->end();
    }

    // open the VBA project storage
    OUString aVbaFragmentPath = getFragmentPathFromFirstType(CREATE_MSOFFICE_RELATION_TYPE("vbaProject"));
    if (!aVbaFragmentPath.isEmpty())
    {
        uno::Reference<io::XInputStream> xInStrm = getFilter().openInputStream(aVbaFragmentPath);
        if (xInStrm.is())
        {
            StorageRef xPrjStrg = std::make_shared<oox::ole::OleStorage>(getFilter().getComponentContext(), xInStrm, false);
            getFilter().getVbaProject().importVbaProject(*xPrjStrg);
        }
    }
}

// CT_Presentation
::oox::core::ContextHandlerRef PresentationFragmentHandler::onCreateContext( sal_Int32 aElementToken, const AttributeList& rAttribs )
{
    switch( aElementToken )
    {
    case PPT_TOKEN( presentation ):
    case PPT_TOKEN( sldMasterIdLst ):
    case PPT_TOKEN( notesMasterIdLst ):
    case PPT_TOKEN( sldIdLst ):
        return this;
    case PPT_TOKEN( sldMasterId ):
        maSlideMasterVector.push_back( rAttribs.getString( R_TOKEN( id ), OUString() ) );
        return this;
    case PPT_TOKEN( sldId ):
        maSlidesVector.push_back( rAttribs.getString( R_TOKEN( id ), OUString() ) );
        return this;
    case PPT_TOKEN( notesMasterId ):
        maNotesMasterVector.push_back( rAttribs.getString( R_TOKEN( id ), OUString() ) );
        return this;
    case PPT_TOKEN( sldSz ):
        maSlideSize = GetSize2D( rAttribs.getFastAttributeList() );
        return this;
    case PPT_TOKEN( notesSz ):
        maNotesSize = GetSize2D( rAttribs.getFastAttributeList() );
        return this;
    case PPT_TOKEN( custShowLst ):
        return new CustomShowListContext( *this, maCustomShowList );
    case PPT_TOKEN( defaultTextStyle ):
        return new TextListStyleContext( *this, *mpTextListStyle );
    case PPT_TOKEN( modifyVerifier ):
        OUString sAlgorithmClass = rAttribs.getString(XML_cryptAlgorithmClass, OUString());
        OUString sAlgorithmType = rAttribs.getString(XML_cryptAlgorithmType, OUString());
        sal_Int32 nAlgorithmSid = rAttribs.getInteger(XML_cryptAlgorithmSid, 0);
        sal_Int32 nSpinCount = rAttribs.getInteger(XML_spinCount, 0);
        OUString sSalt = rAttribs.getString(XML_saltData, OUString());
        OUString sHash = rAttribs.getString(XML_hashData, OUString());
        if (sAlgorithmClass == "hash" && sAlgorithmType == "typeAny" && nAlgorithmSid != 0
            && !sSalt.isEmpty() && !sHash.isEmpty())
        {
            OUString sAlgorithmName;
            switch (nAlgorithmSid)
            {
                case 1:
                    sAlgorithmName = "MD2";
                    break;
                case 2:
                    sAlgorithmName = "MD4";
                    break;
                case 3:
                    sAlgorithmName = "MD5";
                    break;
                case 4:
                    sAlgorithmName = "SHA-1";
                    break;
                case 5:
                    sAlgorithmName = "MAC";
                    break;
                case 6:
                    sAlgorithmName = "RIPEMD";
                    break;
                case 7:
                    sAlgorithmName = "RIPEMD-160";
                    break;
                case 9:
                    sAlgorithmName = "HMAC";
                    break;
                case 12:
                    sAlgorithmName = "SHA-256";
                    break;
                case 13:
                    sAlgorithmName = "SHA-384";
                    break;
                case 14:
                    sAlgorithmName = "SHA-512";
                    break;
                default:; // 8, 10, 11, any other value: Undefined.
            }

            if (!sAlgorithmName.isEmpty())
            {
                uno::Sequence<beans::PropertyValue> aResult{
                    comphelper::makePropertyValue("algorithm-name", sAlgorithmName),
                    comphelper::makePropertyValue("salt", sSalt),
                    comphelper::makePropertyValue("iteration-count", nSpinCount),
                    comphelper::makePropertyValue("hash", sHash)
                };
                try
                {
                    uno::Reference<beans::XPropertySet> xDocSettings(
                        getFilter().getModelFactory()->createInstance(
                            "com.sun.star.document.Settings"),
                        uno::UNO_QUERY);
                    xDocSettings->setPropertyValue("ModifyPasswordInfo", uno::makeAny(aResult));
                }
                catch (const uno::Exception&)
                {
                }
            }
        }
        return this;
    }
    return this;
}

void PresentationFragmentHandler::importSlide( const FragmentHandlerRef& rxSlideFragmentHandler,
        const SlidePersistPtr& rSlidePersistPtr )
{
    Reference< drawing::XDrawPage > xSlide( rSlidePersistPtr->getPage() );
    SlidePersistPtr pMasterPersistPtr( rSlidePersistPtr->getMasterPersist() );
    if ( pMasterPersistPtr )
    {
        // Setting "Layout" property adds extra title and outliner preset shapes to the master slide
        Reference< drawing::XDrawPage > xMasterSlide(pMasterPersistPtr->getPage());
        const int nCount = xMasterSlide->getCount();

        uno::Reference< beans::XPropertySet > xSet( xSlide, uno::UNO_QUERY_THROW );
        xSet->setPropertyValue( "Layout", Any( pMasterPersistPtr->getLayoutFromValueToken() ) );

        while( nCount < xMasterSlide->getCount())
        {
            Reference< drawing::XShape > xShape;
            xMasterSlide->getByIndex(xMasterSlide->getCount()-1) >>= xShape;
            xMasterSlide->remove(xShape);
        }
    }
    while( xSlide->getCount() )
    {
        Reference< drawing::XShape > xShape;
        xSlide->getByIndex(0) >>= xShape;
        xSlide->remove( xShape );
    }

    Reference< XPropertySet > xPropertySet( xSlide, UNO_QUERY );
    if ( xPropertySet.is() )
    {
        awt::Size& rPageSize( rSlidePersistPtr->isNotesPage() ? maNotesSize : maSlideSize );
        xPropertySet->setPropertyValue( "Width", Any( rPageSize.Width ) );
        xPropertySet->setPropertyValue( "Height", Any( rPageSize.Height ) );

        oox::ppt::HeaderFooter aHeaderFooter( rSlidePersistPtr->getHeaderFooter() );
        if ( !rSlidePersistPtr->isMasterPage() )
            aHeaderFooter.mbSlideNumber = aHeaderFooter.mbHeader = aHeaderFooter.mbFooter = aHeaderFooter.mbDateTime = false;
        try
        {
            if ( rSlidePersistPtr->isNotesPage() )
                xPropertySet->setPropertyValue( "IsHeaderVisible", Any( aHeaderFooter.mbHeader ) );
            xPropertySet->setPropertyValue( "IsFooterVisible", Any( aHeaderFooter.mbFooter ) );
            xPropertySet->setPropertyValue( "IsDateTimeVisible", Any( aHeaderFooter.mbDateTime ) );
            xPropertySet->setPropertyValue( "IsPageNumberVisible", Any( aHeaderFooter.mbSlideNumber ) );
        }
        catch( uno::Exception& )
        {
        }
    }
    rSlidePersistPtr->setPath( rxSlideFragmentHandler->getFragmentPath() );
    getFilter().importFragment( rxSlideFragmentHandler );
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
