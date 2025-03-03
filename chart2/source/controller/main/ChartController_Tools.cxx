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

#include <ChartController.hxx>
#include <ChartWindow.hxx>
#include <ChartModel.hxx>
#include <ChartModelHelper.hxx>
#include <ChartType.hxx>
#include <TitleHelper.hxx>
#include <ThreeDHelper.hxx>
#include <DataSeries.hxx>
#include <DataSeriesHelper.hxx>
#include "UndoGuard.hxx"
#include <ControllerLockGuard.hxx>
#include <ResId.hxx>
#include <strings.hrc>
#include <ObjectIdentifier.hxx>
#include <ReferenceSizeProvider.hxx>
#include <chartview/DrawModelWrapper.hxx>
#include "ChartTransferable.hxx"
#include <DrawViewWrapper.hxx>
#include <Legend.hxx>
#include <LegendHelper.hxx>
#include <Axis.hxx>
#include <AxisHelper.hxx>
#include <RegressionCurveModel.hxx>
#include <RegressionCurveHelper.hxx>
#include "ShapeController.hxx"
#include <DiagramHelper.hxx>
#include <Diagram.hxx>
#include <ObjectNameProvider.hxx>
#include <unonames.hxx>

#include <com/sun/star/chart2/DataPointLabel.hpp>
#include <com/sun/star/beans/XPropertyState.hpp>
#include <com/sun/star/graphic/XGraphic.hpp>
#include <com/sun/star/drawing/TextVerticalAdjust.hpp>
#include <com/sun/star/drawing/TextHorizontalAdjust.hpp>
#include <com/sun/star/chart/ErrorBarStyle.hpp>
#include <com/sun/star/chart2/XRegressionCurveContainer.hpp>

#include <editeng/editview.hxx>
#include <editeng/outliner.hxx>
#include <svx/ActionDescriptionProvider.hxx>
#include <vcl/transfer.hxx>
#include <sot/storage.hxx>
#include <vcl/graph.hxx>
#include <vcl/TypeSerializer.hxx>
#include <svx/unomodel.hxx>
#include <svx/svdmodel.hxx>
#include <unotools/streamwrap.hxx>
#include <vcl/svapp.hxx>
#include <svx/dialmgr.hxx>
#include <svx/strings.hrc>
#include <svx/svditer.hxx>
#include <svx/svdpage.hxx>
#include <svx/svdundo.hxx>
#include <svx/unoapi.hxx>
#include <svx/unopage.hxx>
#include <svx/unoshape.hxx>
#include <svx/xgrad.hxx>
#include <PropertyHelper.hxx>

#include <LibreOfficeKit/LibreOfficeKitEnums.h>
#include <tools/debug.hxx>
#include <tools/diagnose_ex.h>
#include <tools/UnitConversion.hxx>

#include <memory>

using namespace ::com::sun::star;

using ::com::sun::star::uno::Reference;
using ::com::sun::star::uno::Sequence;

namespace chart
{

namespace
{

bool lcl_deleteDataSeries(
    const OUString & rCID,
    const rtl::Reference<::chart::ChartModel> & xModel,
    const Reference< document::XUndoManager > & xUndoManager )
{
    bool bResult = false;
    rtl::Reference< DataSeries > xSeries = ObjectIdentifier::getDataSeriesForCID( rCID, xModel );
    if( xSeries.is() && xModel.is())
    {
        rtl::Reference< ::chart::ChartType > xChartType =
            DataSeriesHelper::getChartTypeOfSeries( xSeries, xModel->getFirstChartDiagram());
        if( xChartType.is())
        {
            UndoGuard aUndoGuard(
                ActionDescriptionProvider::createDescription(
                    ActionDescriptionProvider::ActionType::Delete, SchResId( STR_OBJECT_DATASERIES )),
                xUndoManager );

            rtl::Reference< Diagram > xDiagram = ChartModelHelper::findDiagram( xModel );
            rtl::Reference< Axis > xAxis = DiagramHelper::getAttachedAxis( xSeries, xDiagram );

            DataSeriesHelper::deleteSeries( xSeries, xChartType );

            AxisHelper::hideAxisIfNoDataIsAttached( xAxis, xDiagram );

            bResult = true;
            aUndoGuard.commit();
        }
    }
    return bResult;
}

bool lcl_deleteDataCurve(
    const OUString & rCID,
    const rtl::Reference<::chart::ChartModel> & xModel,
    const Reference< document::XUndoManager > & xUndoManager )
{
    bool bResult = false;

    uno::Reference< beans::XPropertySet > xProperties(
        ObjectIdentifier::getObjectPropertySet( rCID, xModel));

    uno::Reference< chart2::XRegressionCurve > xRegressionCurve( xProperties, uno::UNO_QUERY );

    if( xRegressionCurve.is())
    {
        uno::Reference< chart2::XRegressionCurveContainer > xRegressionCurveContainer(
            ObjectIdentifier::getObjectPropertySet(
                OUString(ObjectIdentifier::getFullParentParticle( rCID )), xModel), uno::UNO_QUERY );

        if( xRegressionCurveContainer.is())
        {
            UndoGuard aUndoGuard(
                ActionDescriptionProvider::createDescription(
                    ActionDescriptionProvider::ActionType::Delete, SchResId( STR_OBJECT_CURVE )),
                xUndoManager );

            xRegressionCurveContainer->removeRegressionCurve( xRegressionCurve );

            bResult = true;
            aUndoGuard.commit();
        }
    }
    return bResult;
}

} // anonymous namespace

std::unique_ptr<ReferenceSizeProvider> ChartController::impl_createReferenceSizeProvider()
{
    awt::Size aPageSize( ChartModelHelper::getPageSize( getChartModel() ) );

    return std::make_unique<ReferenceSizeProvider>(aPageSize, getChartModel());
}

void ChartController::impl_adaptDataSeriesAutoResize()
{
    std::unique_ptr<ReferenceSizeProvider> pRefSizeProvider(impl_createReferenceSizeProvider());
    if (pRefSizeProvider)
        pRefSizeProvider->setValuesAtAllDataSeries();
}

void ChartController::executeDispatch_NewArrangement()
{
    // remove manual positions at titles, legend and the diagram, remove manual
    // size at the diagram

    try
    {
        rtl::Reference<::chart::ChartModel> xModel( getChartModel() );
        rtl::Reference< Diagram > xDiagram = ChartModelHelper::findDiagram( xModel );
        if( xDiagram.is())
        {
            UndoGuard aUndoGuard(
                SchResId( STR_ACTION_REARRANGE_CHART ),
                m_xUndoManager );
            ControllerLockGuardUNO aCtlLockGuard( xModel );

            // diagram
            xDiagram->setPropertyToDefault( "RelativeSize");
            xDiagram->setPropertyToDefault( "RelativePosition");
            xDiagram->setPropertyToDefault( "PosSizeExcludeAxes");

            // 3d rotation
            ThreeDHelper::set3DSettingsToDefault( xDiagram );

            // legend
            Reference< beans::XPropertyState > xLegendState( xDiagram->getLegend(), uno::UNO_QUERY );
            if( xLegendState.is())
            {
                xLegendState->setPropertyToDefault( "RelativePosition");
                xLegendState->setPropertyToDefault( "RelativeSize");
                xLegendState->setPropertyToDefault( "AnchorPosition");
            }

            // titles
            for( sal_Int32 eType = TitleHelper::TITLE_BEGIN;
                 eType < TitleHelper::NORMAL_TITLE_END;
                 ++eType )
            {
                Reference< beans::XPropertyState > xTitleState(
                    TitleHelper::getTitle(
                        static_cast< TitleHelper::eTitleType >( eType ), xModel ), uno::UNO_QUERY );
                if( xTitleState.is())
                    xTitleState->setPropertyToDefault( "RelativePosition");
            }

            // regression curve equations
            std::vector< rtl::Reference< RegressionCurveModel > > aRegressionCurves =
                RegressionCurveHelper::getAllRegressionCurvesNotMeanValueLine( xDiagram );

            // reset equation position
            for( const auto& xCurve : aRegressionCurves )
                RegressionCurveHelper::resetEquationPosition( xCurve );

            aUndoGuard.commit();
        }
    }
    catch( const uno::RuntimeException & )
    {
        DBG_UNHANDLED_EXCEPTION("chart2");
    }
}

void ChartController::executeDispatch_ScaleText()
{
    SolarMutexGuard aSolarGuard;
    UndoGuard aUndoGuard(
        SchResId( STR_ACTION_SCALE_TEXT ),
        m_xUndoManager );
    ControllerLockGuardUNO aCtlLockGuard( getChartModel() );

    std::unique_ptr<ReferenceSizeProvider> pRefSizeProv(impl_createReferenceSizeProvider());
    OSL_ASSERT(pRefSizeProv);
    if (pRefSizeProv)
        pRefSizeProv->toggleAutoResizeState();

    aUndoGuard.commit();
}

void ChartController::executeDispatch_Paste()
{
    SolarMutexGuard aGuard;
    auto pChartWindow(GetChartWindow());
    if( !pChartWindow )
        return;

    Graphic aGraphic;
    // paste location: center of window
    Point aPos = pChartWindow->PixelToLogic( tools::Rectangle( {}, pChartWindow->GetSizePixel()).Center());

    // handle different formats
    TransferableDataHelper aDataHelper( TransferableDataHelper::CreateFromSystemClipboard( pChartWindow ));
    if( aDataHelper.GetTransferable().is())
    {
        if ( aDataHelper.HasFormat( SotClipboardFormatId::DRAWING ) )
        {
            tools::SvRef<SotTempStream> xStm;
            if ( aDataHelper.GetSotStorageStream( SotClipboardFormatId::DRAWING, xStm ) )
            {
                xStm->Seek( 0 );
                Reference< io::XInputStream > xInputStream( new utl::OInputStreamWrapper( *xStm ) );

                std::unique_ptr< SdrModel > spModel(
                    new SdrModel());

                if ( SvxDrawingLayerImport( spModel.get(), xInputStream ) )
                {
                    impl_PasteShapes( spModel.get() );
                }
            }
        }
        else if ( aDataHelper.HasFormat( SotClipboardFormatId::SVXB ) )
        {
            // graphic exchange format (graphic manager bitmap format?)
            tools::SvRef<SotTempStream> xStm;
            if( aDataHelper.GetSotStorageStream( SotClipboardFormatId::SVXB, xStm ))
            {
                TypeSerializer aSerializer(*xStm);
                aSerializer.readGraphic(aGraphic);
            }
        }
        else if( aDataHelper.HasFormat( SotClipboardFormatId::GDIMETAFILE ))
        {
            // meta file
            GDIMetaFile aMetafile;
            if( aDataHelper.GetGDIMetaFile( SotClipboardFormatId::GDIMETAFILE, aMetafile ))
                aGraphic = Graphic( aMetafile );
        }
        else if( aDataHelper.HasFormat( SotClipboardFormatId::BITMAP ))
        {
            // bitmap (non-graphic-manager)
            BitmapEx aBmpEx;
            if( aDataHelper.GetBitmapEx( SotClipboardFormatId::BITMAP, aBmpEx ))
                aGraphic = Graphic( aBmpEx );
        }
        else if( aDataHelper.HasFormat( SotClipboardFormatId::STRING ))
        {
            OUString aString;
            if( aDataHelper.GetString( SotClipboardFormatId::STRING, aString ) && m_pDrawModelWrapper )
            {
                if( m_pDrawViewWrapper )
                {
                    OutlinerView* pOutlinerView = m_pDrawViewWrapper->GetTextEditOutlinerView();
                    if( pOutlinerView )//in case of edit mode insert into edited string
                        pOutlinerView->InsertText( aString );
                    else
                    {
                        impl_PasteStringAsTextShape( aString, awt::Point( 0, 0 ) );
                    }
                }
            }
        }
    }

    if( aGraphic.GetType() != GraphicType::NONE )
    {
        Reference< graphic::XGraphic > xGraphic( aGraphic.GetXGraphic());
        if( xGraphic.is())
            impl_PasteGraphic( xGraphic, aPos );
    }
}

// note: aPosition is ignored for now. The object is always pasted centered to
// the page
void ChartController::impl_PasteGraphic(
    uno::Reference< graphic::XGraphic > const & xGraphic,
    const ::Point & /* aPosition */ )
{
    DBG_TESTSOLARMUTEX();
    // note: the XPropertySet of the model is the old API. Also the property
    // "AdditionalShapes" that is used there.
    rtl::Reference< ChartModel > xModel = getChartModel();
    DrawModelWrapper * pDrawModelWrapper( GetDrawModelWrapper());
    if( ! (xGraphic.is() && xModel.is()))
        return;
    rtl::Reference<SvxGraphicObject> xGraphicShape = new SvxGraphicObject(nullptr);
    xGraphicShape->setShapeKind(SdrObjKind::Graphic);

    uno::Reference< drawing::XShapes > xPage = pDrawModelWrapper->getMainDrawPage();
    if( xPage.is())
    {
        xPage->add( xGraphicShape );
        //need to change the model state manually
        xModel->setModified( true );
        //select new shape
        m_aSelection.setSelection( xGraphicShape );
        m_aSelection.applySelection( m_pDrawViewWrapper.get() );
    }
    xGraphicShape->SvxShape::setPropertyValue( "Graphic", uno::Any( xGraphic ));

    awt::Size aGraphicSize( 1000, 1000 );
    auto pChartWindow(GetChartWindow());
    // first try size in 100th mm, then pixel size
    if( ! ( xGraphicShape->SvxShape::getPropertyValue( "Size100thMM") >>= aGraphicSize ) &&
        ( ( xGraphicShape->SvxShape::getPropertyValue( "SizePixel") >>= aGraphicSize ) && pChartWindow ))
    {
        ::Size aVCLSize( pChartWindow->PixelToLogic( Size( aGraphicSize.Width, aGraphicSize.Height )));
        aGraphicSize.Width = aVCLSize.getWidth();
        aGraphicSize.Height = aVCLSize.getHeight();
    }
    xGraphicShape->setSize( aGraphicSize );
    xGraphicShape->setPosition( awt::Point( 0, 0 ) );
}

void ChartController::impl_PasteShapes( SdrModel* pModel )
{
    DrawModelWrapper* pDrawModelWrapper( GetDrawModelWrapper() );
    if ( !(pDrawModelWrapper && m_pDrawViewWrapper) )
        return;

    Reference< drawing::XDrawPage > xDestPage( pDrawModelWrapper->getMainDrawPage() );
    SdrPage* pDestPage = GetSdrPageFromXDrawPage( xDestPage );
    if ( !pDestPage )
        return;

    Reference< drawing::XShape > xSelShape;
    m_pDrawViewWrapper->BegUndo( SvxResId( RID_SVX_3D_UNDO_EXCHANGE_PASTE ) );
    sal_uInt16 nCount = pModel->GetPageCount();
    for ( sal_uInt16 i = 0; i < nCount; ++i )
    {
        const SdrPage* pPage = pModel->GetPage( i );
        SdrObjListIter aIter( pPage, SdrIterMode::DeepNoGroups );
        while ( aIter.IsMore() )
        {
            SdrObject* pObj(aIter.Next());
            // Clone to new SdrModel
            SdrObject* pNewObj(pObj ? pObj->CloneSdrObject(pDrawModelWrapper->getSdrModel()) : nullptr);

            if ( pNewObj )
            {
                // set position
                Reference< drawing::XShape > xShape( pNewObj->getUnoShape(), uno::UNO_QUERY );
                if ( xShape.is() )
                {
                    xShape->setPosition( awt::Point( 0, 0 ) );
                }

                pDestPage->InsertObject( pNewObj );
                m_pDrawViewWrapper->AddUndo( std::make_unique<SdrUndoInsertObj>( *pNewObj ) );
                xSelShape = xShape;
            }
        }
    }

    rtl::Reference< ChartModel > xModifiable = getChartModel();
    if ( xModifiable.is() )
    {
        xModifiable->setModified( true );
    }

    // select last inserted shape
    m_aSelection.setSelection( xSelShape );
    m_aSelection.applySelection( m_pDrawViewWrapper.get() );

    m_pDrawViewWrapper->EndUndo();

    impl_switchDiagramPositioningToExcludingPositioning();
}

void ChartController::impl_PasteStringAsTextShape( const OUString& rString, const awt::Point& rPosition )
{
    DrawModelWrapper* pDrawModelWrapper( GetDrawModelWrapper() );
    if ( !(pDrawModelWrapper && m_pDrawViewWrapper) )
        return;

    const Reference< drawing::XDrawPage >& xDrawPage( pDrawModelWrapper->getMainDrawPage() );
    OSL_ASSERT( xDrawPage.is() );

    if ( !xDrawPage )
        return;

    try
    {
        rtl::Reference<SvxShapeText> xTextShape = new SvxShapeText(nullptr);
        xTextShape->setShapeKind(SdrObjKind::Text);
        xDrawPage->add( xTextShape );

        xTextShape->setString( rString );

        float fCharHeight = 10.0;
        xTextShape->SvxShape::setPropertyValue( "TextAutoGrowHeight", uno::Any( true ) );
        xTextShape->SvxShape::setPropertyValue( "TextAutoGrowWidth", uno::Any( true ) );
        xTextShape->SvxShape::setPropertyValue( "CharHeight", uno::Any( fCharHeight ) );
        xTextShape->SvxShape::setPropertyValue( "CharHeightAsian", uno::Any( fCharHeight ) );
        xTextShape->SvxShape::setPropertyValue( "CharHeightComplex", uno::Any( fCharHeight ) );
        xTextShape->SvxShape::setPropertyValue( "TextVerticalAdjust", uno::Any( drawing::TextVerticalAdjust_CENTER ) );
        xTextShape->SvxShape::setPropertyValue( "TextHorizontalAdjust", uno::Any( drawing::TextHorizontalAdjust_CENTER ) );
        xTextShape->SvxShape::setPropertyValue( "CharFontName", uno::Any( OUString("Albany") ) );

        xTextShape->setPosition( rPosition );

        m_aSelection.setSelection( xTextShape );
        m_aSelection.applySelection( m_pDrawViewWrapper.get() );

        SdrObject* pObj = DrawViewWrapper::getSdrObject( xTextShape );
        if ( pObj )
        {
            m_pDrawViewWrapper->BegUndo( SvxResId( RID_SVX_3D_UNDO_EXCHANGE_PASTE ) );
            m_pDrawViewWrapper->AddUndo( std::make_unique<SdrUndoInsertObj>( *pObj ) );
            m_pDrawViewWrapper->EndUndo();

            impl_switchDiagramPositioningToExcludingPositioning();
        }
    }
    catch ( const uno::Exception& )
    {
        DBG_UNHANDLED_EXCEPTION("chart2");
    }
}

void ChartController::executeDispatch_Copy()
{
    SolarMutexGuard aSolarGuard;
    if (!m_pDrawViewWrapper)
        return;

    OutlinerView* pOutlinerView = m_pDrawViewWrapper->GetTextEditOutlinerView();
    if (pOutlinerView)
        pOutlinerView->Copy();
    else
    {
        SdrObject* pSelectedObj = nullptr;
        ObjectIdentifier aSelOID(m_aSelection.getSelectedOID());

        if (aSelOID.isAutoGeneratedObject())
            pSelectedObj = m_pDrawModelWrapper->getNamedSdrObject( aSelOID.getObjectCID() );
        else if (aSelOID.isAdditionalShape())
            pSelectedObj = DrawViewWrapper::getSdrObject( aSelOID.getAdditionalShape() );

        if (pSelectedObj)
        {
            Reference<datatransfer::clipboard::XClipboard> xClipboard(GetChartWindow()->GetClipboard());
            if (xClipboard.is())
            {
                Reference< datatransfer::XTransferable > xTransferable(
                    new ChartTransferable(m_pDrawModelWrapper->getSdrModel(),
                                          pSelectedObj, aSelOID.isAdditionalShape()));
                xClipboard->setContents(xTransferable, Reference< datatransfer::clipboard::XClipboardOwner >());
            }
        }
    }
}

void ChartController::executeDispatch_Cut()
{
    executeDispatch_Copy();
    executeDispatch_Delete();
}

bool ChartController::isObjectDeleteable( const uno::Any& rSelection )
{
    ObjectIdentifier aSelOID( rSelection );
    if ( aSelOID.isAutoGeneratedObject() )
    {
        const OUString& aSelObjCID( aSelOID.getObjectCID() );
        ObjectType aObjectType(ObjectIdentifier::getObjectType( aSelObjCID ));

        switch(aObjectType)
        {
        case OBJECTTYPE_TITLE:
        case OBJECTTYPE_LEGEND:
        case OBJECTTYPE_DATA_SERIES:
        case OBJECTTYPE_LEGEND_ENTRY:
        case OBJECTTYPE_DATA_CURVE_EQUATION:
        case OBJECTTYPE_DATA_CURVE:
        case OBJECTTYPE_DATA_AVERAGE_LINE:
        case OBJECTTYPE_DATA_ERRORS_X:
        case OBJECTTYPE_DATA_ERRORS_Y:
        case OBJECTTYPE_DATA_ERRORS_Z:
        case OBJECTTYPE_DATA_LABELS:
        case OBJECTTYPE_DATA_LABEL:
        case OBJECTTYPE_AXIS:
        case OBJECTTYPE_GRID:
        case OBJECTTYPE_SUBGRID:
            return true;
        default:
            break;
        }
    }
    else if ( aSelOID.isAdditionalShape() )
    {
        return true;
    }

    return false;
}

bool ChartController::isShapeContext() const
{
    return m_aSelection.isAdditionalShapeSelected() ||
         ( m_pDrawViewWrapper && m_pDrawViewWrapper->AreObjectsMarked() &&
           ( m_pDrawViewWrapper->GetCurrentObjIdentifier() == SdrObjKind::Text ) );
}

void ChartController::impl_ClearSelection()
{
    if( m_aSelection.hasSelection())
    {
        m_aSelection.clearSelection();
        impl_notifySelectionChangeListeners();
    }
}

bool ChartController::executeDispatch_Delete()
{
    bool bReturn = false;

    // remove the selected object
    OUString aCID( m_aSelection.getSelectedCID() );
    if( !aCID.isEmpty() )
    {
        if( !isObjectDeleteable( uno::Any( aCID ) ) )
            return false;

        //remove chart object
        rtl::Reference< ChartModel > xChartDoc = getChartModel();
        if( !xChartDoc.is() )
            return false;

        ObjectType aObjectType( ObjectIdentifier::getObjectType( aCID ));
        switch( aObjectType )
        {
            case OBJECTTYPE_TITLE:
            {
                UndoGuard aUndoGuard(
                    ActionDescriptionProvider::createDescription(
                        ActionDescriptionProvider::ActionType::Delete, SchResId( STR_OBJECT_TITLE )),
                    m_xUndoManager );
                TitleHelper::removeTitle(
                    ObjectIdentifier::getTitleTypeForCID( aCID ), getChartModel() );
                bReturn = true;
                aUndoGuard.commit();
                break;
            }
            case OBJECTTYPE_LEGEND:
            {
                rtl::Reference< Diagram > xDiagram( xChartDoc->getFirstChartDiagram());
                if( xDiagram.is())
                {
                    uno::Reference< beans::XPropertySet > xLegendProp( xDiagram->getLegend(), uno::UNO_QUERY );
                    if( xLegendProp.is())
                    {
                        UndoGuard aUndoGuard(
                            ActionDescriptionProvider::createDescription(
                                ActionDescriptionProvider::ActionType::Delete, SchResId( STR_OBJECT_LEGEND )),
                            m_xUndoManager );
                        xLegendProp->setPropertyValue( "Show", uno::Any( false ));
                        bReturn = true;
                        aUndoGuard.commit();
                    }
                }
                break;
            }

            case OBJECTTYPE_DATA_SERIES:
                bReturn = lcl_deleteDataSeries( aCID, getChartModel(), m_xUndoManager );
                break;

            case OBJECTTYPE_LEGEND_ENTRY:
            {
                ObjectType eParentObjectType = ObjectIdentifier::getObjectType(
                    ObjectIdentifier::getFullParentParticle( aCID ));
                if( eParentObjectType == OBJECTTYPE_DATA_SERIES )
                {
                    bReturn = lcl_deleteDataSeries( aCID, getChartModel(), m_xUndoManager );
                }
                else if( eParentObjectType == OBJECTTYPE_DATA_CURVE )
                {
                    sal_Int32 nEndPos = aCID.lastIndexOf(':');
                    OUString aParentCID = aCID.copy(0, nEndPos);

                    bReturn = lcl_deleteDataCurve(aParentCID, getChartModel(), m_xUndoManager );
                }
                else if( eParentObjectType == OBJECTTYPE_DATA_AVERAGE_LINE )
                {
                    executeDispatch_DeleteMeanValue();
                    bReturn = true;
                }
                break;
            }

            case OBJECTTYPE_DATA_AVERAGE_LINE:
            {
                uno::Reference< chart2::XRegressionCurveContainer > xRegCurveCnt(
                    ObjectIdentifier::getObjectPropertySet(
                        OUString(ObjectIdentifier::getFullParentParticle( aCID )), getChartModel()), uno::UNO_QUERY );
                if( xRegCurveCnt.is())
                {
                    UndoGuard aUndoGuard(
                        ActionDescriptionProvider::createDescription(
                            ActionDescriptionProvider::ActionType::Delete, SchResId( STR_OBJECT_AVERAGE_LINE )),
                        m_xUndoManager );
                    RegressionCurveHelper::removeMeanValueLine( xRegCurveCnt );
                    bReturn = true;
                    aUndoGuard.commit();
                }
            }
            break;

            case OBJECTTYPE_DATA_CURVE:
            {
                bReturn = lcl_deleteDataCurve( aCID, getChartModel(), m_xUndoManager );
            }
            break;

            case OBJECTTYPE_DATA_CURVE_EQUATION:
            {
                uno::Reference< beans::XPropertySet > xEqProp(
                    ObjectIdentifier::getObjectPropertySet( aCID, getChartModel()));

                if( xEqProp.is())
                {
                    rtl::Reference<::chart::ChartModel> xModel( getChartModel() );
                    UndoGuard aUndoGuard(
                        ActionDescriptionProvider::createDescription(
                            ActionDescriptionProvider::ActionType::Delete, SchResId( STR_OBJECT_CURVE_EQUATION )),
                        m_xUndoManager );
                    {
                        ControllerLockGuardUNO aCtlLockGuard( xModel );
                        xEqProp->setPropertyValue( "ShowEquation", uno::Any( false ));
                        xEqProp->setPropertyValue( "XName", uno::Any( OUString("x") ));
                        xEqProp->setPropertyValue( "YName", uno::Any( OUString("f(x)") ));
                        xEqProp->setPropertyValue( "ShowCorrelationCoefficient", uno::Any( false ));
                    }
                    bReturn = true;
                    aUndoGuard.commit();
                }
            }
            break;

            case OBJECTTYPE_DATA_ERRORS_X:
            case OBJECTTYPE_DATA_ERRORS_Y:
            case OBJECTTYPE_DATA_ERRORS_Z:
            {
                uno::Reference< beans::XPropertySet > xErrorBarProp(
                    ObjectIdentifier::getObjectPropertySet( aCID, getChartModel() ));
                if( xErrorBarProp.is())
                {
                    TranslateId pId;

                    if ( aObjectType == OBJECTTYPE_DATA_ERRORS_X )
                        pId = STR_OBJECT_ERROR_BARS_X;
                    else if ( aObjectType == OBJECTTYPE_DATA_ERRORS_Y )
                        pId = STR_OBJECT_ERROR_BARS_Y;
                    else
                        pId = STR_OBJECT_ERROR_BARS_Z;

                    rtl::Reference<::chart::ChartModel> xModel( getChartModel() );
                    UndoGuard aUndoGuard(
                        ActionDescriptionProvider::createDescription(
                            ActionDescriptionProvider::ActionType::Delete, SchResId(pId)),
                        m_xUndoManager);
                    {
                        ControllerLockGuardUNO aCtlLockGuard( xModel );
                        xErrorBarProp->setPropertyValue(
                            "ErrorBarStyle",
                            uno::Any( css::chart::ErrorBarStyle::NONE ));
                    }
                    bReturn = true;
                    aUndoGuard.commit();
                }
                break;
            }

            case OBJECTTYPE_DATA_LABELS:
            case OBJECTTYPE_DATA_LABEL:
            {
                uno::Reference< beans::XPropertySet > xObjectProperties =
                    ObjectIdentifier::getObjectPropertySet( aCID, getChartModel() );
                if( xObjectProperties.is() )
                {
                    UndoGuard aUndoGuard(
                        ActionDescriptionProvider::createDescription(
                        ActionDescriptionProvider::ActionType::Delete,
                            SchResId( aObjectType == OBJECTTYPE_DATA_LABEL ? STR_OBJECT_LABEL : STR_OBJECT_DATALABELS )),
                                m_xUndoManager );
                    chart2::DataPointLabel aLabel;
                    xObjectProperties->getPropertyValue(CHART_UNONAME_LABEL) >>= aLabel;
                    aLabel.ShowNumber = false;
                    aLabel.ShowNumberInPercent = false;
                    aLabel.ShowCategoryName = false;
                    aLabel.ShowLegendSymbol = false;
                    aLabel.ShowCustomLabel = false;
                    aLabel.ShowSeriesName = false;
                    if( aObjectType == OBJECTTYPE_DATA_LABELS )
                    {
                        rtl::Reference< DataSeries > xSeries = ObjectIdentifier::getDataSeriesForCID( aCID, getChartModel() );
                        DataSeriesHelper::setPropertyAlsoToAllAttributedDataPoints( xSeries, CHART_UNONAME_LABEL, uno::Any(aLabel) );
                        DataSeriesHelper::setPropertyAlsoToAllAttributedDataPoints( xSeries, CHART_UNONAME_CUSTOM_LABEL_FIELDS, uno::Any() );
                    }
                    else
                    {
                        xObjectProperties->setPropertyValue(CHART_UNONAME_LABEL, uno::Any(aLabel));
                        xObjectProperties->setPropertyValue(CHART_UNONAME_CUSTOM_LABEL_FIELDS, uno::Any());
                    }
                    bReturn = true;
                    aUndoGuard.commit();
                }
                break;
            }
            case OBJECTTYPE_AXIS:
            {
                executeDispatch_DeleteAxis();
                bReturn = true;
                break;
            }
            case OBJECTTYPE_GRID:
            {
                executeDispatch_DeleteMajorGrid();
                bReturn = true;
                break;
            }
            case OBJECTTYPE_SUBGRID:
            {
                executeDispatch_DeleteMinorGrid();
                bReturn = true;
                break;
            }

            default:
            {
                break;
            }
        }
    }
    else
    {
        //remove additional shape
        impl_ClearSelection();
        {
            SolarMutexGuard aSolarGuard;
            if ( m_pDrawViewWrapper )
            {
                m_pDrawViewWrapper->DeleteMarked();
                bReturn = true;
            }
        }
    }
    return bReturn;
}

void ChartController::executeDispatch_ToggleLegend()
{
    rtl::Reference< ChartModel > xModel = getChartModel();
    UndoGuard aUndoGuard(
        SchResId( STR_ACTION_TOGGLE_LEGEND ), m_xUndoManager );
    rtl::Reference< Legend > xLegendProp = LegendHelper::getLegend(*xModel);
    bool bChanged = false;
    if( xLegendProp.is())
    {
        try
        {
            bool bShow = false;
            if( xLegendProp->getPropertyValue( "Show") >>= bShow )
            {
                xLegendProp->setPropertyValue( "Show", uno::Any( ! bShow ));
                bChanged = true;
            }
        }
        catch( const uno::Exception & )
        {
            DBG_UNHANDLED_EXCEPTION("chart2");
        }
    }
    else
    {
        xLegendProp = LegendHelper::getLegend(*xModel, m_xCC, true);
        if( xLegendProp.is())
            bChanged = true;
    }

    if( bChanged )
        aUndoGuard.commit();
}

void ChartController::executeDispatch_ToggleGridHorizontal()
{
    UndoGuard aUndoGuard(
        SchResId( STR_ACTION_TOGGLE_GRID_HORZ ), m_xUndoManager );
    rtl::Reference< Diagram > xDiagram( getFirstDiagram() );
    if( !xDiagram.is())
        return;

    sal_Int32 nDimensionIndex = 1;
    sal_Int32 nCooSysIndex = 0;

    bool bHasMajorYGrid = AxisHelper::isGridShown( nDimensionIndex, nCooSysIndex, true,  xDiagram );
    bool bHasMinorYGrid = AxisHelper::isGridShown( nDimensionIndex, nCooSysIndex, false, xDiagram );

    if( bHasMajorYGrid )
    {
        if ( bHasMinorYGrid )
        {
            AxisHelper::hideGrid( nDimensionIndex, nCooSysIndex, true,  xDiagram );
            AxisHelper::hideGrid( nDimensionIndex, nCooSysIndex, false, xDiagram );
        }
        else
        {
            AxisHelper::showGrid( nDimensionIndex, nCooSysIndex, false, xDiagram );
        }
    }
    else
    {
        AxisHelper::showGrid( nDimensionIndex, nCooSysIndex, true, xDiagram );
    }
    aUndoGuard.commit();
}

void ChartController::executeDispatch_ToggleGridVertical()
{
    UndoGuard aUndoGuard(
        SchResId( STR_ACTION_TOGGLE_GRID_VERTICAL ), m_xUndoManager );
    rtl::Reference< Diagram > xDiagram( getFirstDiagram() );
    if( !xDiagram.is())
        return;

    sal_Int32 nDimensionIndex = 0;
    sal_Int32 nCooSysIndex = 0;

    bool bHasMajorXGrid = AxisHelper::isGridShown( nDimensionIndex, nCooSysIndex, true,  xDiagram );
    bool bHasMinorXGrid = AxisHelper::isGridShown( nDimensionIndex, nCooSysIndex, false, xDiagram );
    if( bHasMajorXGrid )
    {
        if (bHasMinorXGrid)
        {
            AxisHelper::hideGrid( nDimensionIndex, nCooSysIndex, true,  xDiagram );
            AxisHelper::hideGrid( nDimensionIndex, nCooSysIndex, false, xDiagram );
        }
        else
        {
            AxisHelper::showGrid( nDimensionIndex, nCooSysIndex, false, xDiagram );
        }
    }
    else
    {
        AxisHelper::showGrid( nDimensionIndex, nCooSysIndex, true, xDiagram );
    }

    aUndoGuard.commit();
}

void ChartController::executeDispatch_FillColor(sal_uInt32 nColor)
{
    try
    {
        OUString aCID( m_aSelection.getSelectedCID() );
        rtl::Reference<::chart::ChartModel> xChartModel = getChartModel();
        if( xChartModel.is() )
        {
            Reference< beans::XPropertySet > xPointProperties(
                ObjectIdentifier::getObjectPropertySet( aCID, xChartModel ) );
            if( xPointProperties.is() )
                xPointProperties->setPropertyValue( "FillColor", uno::Any( nColor ) );
        }
    }
    catch( const uno::Exception& )
    {
        DBG_UNHANDLED_EXCEPTION( "chart2" );
    }
}

void ChartController::executeDispatch_FillGradient(OUString sJSONGradient)
{
    XGradient aXGradient = XGradient::fromJSON(sJSONGradient);
    css::awt::Gradient aGradient = aXGradient.toGradientUNO();

    try
    {
        OUString aCID( m_aSelection.getSelectedCID() );
        rtl::Reference<::chart::ChartModel> xChartModel = getChartModel();

        if( xChartModel.is() )
        {
            Reference< beans::XPropertySet > xPropSet(
                ObjectIdentifier::getObjectPropertySet( aCID, xChartModel ) );

            if( xPropSet.is() )
            {
                OUString aPrefferedName = OUString::number(static_cast<sal_Int32>(aXGradient.GetStartColor()))
                                + OUString::number(static_cast<sal_Int32>(aXGradient.GetEndColor()))
                                + OUString::number(static_cast<sal_Int32>(aXGradient.GetAngle().get()));

                OUString aNewName = PropertyHelper::addGradientUniqueNameToTable(css::uno::Any(aGradient),
                                        xChartModel,
                                        aPrefferedName);

                xPropSet->setPropertyValue("FillGradientName", css::uno::Any(aNewName));
            }
        }
    }
    catch( const uno::Exception & )
    {
        TOOLS_WARN_EXCEPTION("chart2", "" );
    }
}

void ChartController::executeDispatch_LineColor(sal_uInt32 nColor)
{
    try
    {
        OUString aCID( m_aSelection.getSelectedCID() );
        rtl::Reference<::chart::ChartModel> xChartModel = getChartModel();
        if( xChartModel.is() )
        {
            Reference< beans::XPropertySet > xPropSet(
                ObjectIdentifier::getObjectPropertySet( aCID, xChartModel ) );

            ObjectType eType = ObjectIdentifier::getObjectType(aCID);
            if (eType == OBJECTTYPE_DIAGRAM)
            {
                css::uno::Reference<css::chart2::XDiagram> xDiagram(
                        xPropSet, css::uno::UNO_QUERY);
                if (xDiagram.is())
                    xPropSet.set(xDiagram->getWall());
            }

            if( xPropSet.is() )
                xPropSet->setPropertyValue( "LineColor", css::uno::makeAny( Color(ColorTransparency, nColor) ) );
        }
    }
    catch( const uno::Exception& )
    {
        DBG_UNHANDLED_EXCEPTION( "chart2" );
    }
}

void ChartController::executeDispatch_LineWidth(sal_uInt32 nWidth)
{
    try
    {
        OUString aCID( m_aSelection.getSelectedCID() );
        rtl::Reference<::chart::ChartModel> xChartModel = getChartModel();
        if( xChartModel.is() )
        {
            Reference< beans::XPropertySet > xPropSet(
                ObjectIdentifier::getObjectPropertySet( aCID, xChartModel ) );

            ObjectType eType = ObjectIdentifier::getObjectType(aCID);
            if (eType == OBJECTTYPE_DIAGRAM)
            {
                css::uno::Reference<css::chart2::XDiagram> xDiagram(
                        xPropSet, css::uno::UNO_QUERY);
                if (xDiagram.is())
                    xPropSet.set(xDiagram->getWall());
            }

            if( xPropSet.is() )
                xPropSet->setPropertyValue( "LineWidth", css::uno::makeAny( nWidth ) );
        }
    }
    catch( const uno::Exception& )
    {
        DBG_UNHANDLED_EXCEPTION( "chart2" );
    }
}

void ChartController::executeDispatch_LOKSetTextSelection(int nType, int nX, int nY)
{
    if (!m_pDrawViewWrapper)
        return;

    if (!m_pDrawViewWrapper->IsTextEdit())
        return;

    OutlinerView* pOutlinerView = m_pDrawViewWrapper->GetTextEditOutlinerView();
    if (!pOutlinerView)
        return;

    EditView& rEditView = pOutlinerView->GetEditView();
    Point aPoint(convertTwipToMm100(nX), convertTwipToMm100(nY));
    switch (nType)
    {
        case LOK_SETTEXTSELECTION_START:
            rEditView.SetCursorLogicPosition(aPoint, /*bPoint=*/false, /*bClearMark=*/false);
            break;
        case LOK_SETTEXTSELECTION_END:
            rEditView.SetCursorLogicPosition(aPoint, /*bPoint=*/true, /*bClearMark=*/false);
            break;
        case LOK_SETTEXTSELECTION_RESET:
            rEditView.SetCursorLogicPosition(aPoint, /*bPoint=*/true, /*bClearMark=*/true);
            break;
        default:
            assert(false);
            break;
    }
}

void ChartController::executeDispatch_LOKPieSegmentDragging( int nOffset )
{
    try
    {
        OUString aCID( m_aSelection.getSelectedCID() );
        rtl::Reference<::chart::ChartModel> xChartModel = getChartModel();
        if( xChartModel.is() )
        {
            Reference< beans::XPropertySet > xPointProperties(
                ObjectIdentifier::getObjectPropertySet( aCID, xChartModel ) );
            if( xPointProperties.is() )
                xPointProperties->setPropertyValue( "Offset", uno::Any( nOffset / 100.0 ) );
        }
    }
    catch( const uno::Exception & )
    {
        TOOLS_WARN_EXCEPTION("chart2", "" );
    }
}

void ChartController::impl_ShapeControllerDispatch( const util::URL& rURL, const Sequence< beans::PropertyValue >& rArgs )
{
    Reference< frame::XDispatch > xDispatch( m_aDispatchContainer.getShapeController() );
    if ( xDispatch.is() )
    {
        xDispatch->dispatch( rURL, rArgs );
    }
}

void ChartController::impl_switchDiagramPositioningToExcludingPositioning()
{
    UndoGuard aUndoGuard( ActionDescriptionProvider::createDescription(
        ActionDescriptionProvider::ActionType::PosSize,
        ObjectNameProvider::getName( OBJECTTYPE_DIAGRAM)),
        m_xUndoManager );
    if (DiagramHelper::switchDiagramPositioningToExcludingPositioning(*getChartModel(), true, true))
        aUndoGuard.commit();
}

} //  namespace chart

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
