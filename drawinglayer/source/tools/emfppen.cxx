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

#include <com/sun/star/rendering/PathCapType.hpp>
#include <o3tl/safeint.hxx>
#include <sal/log.hxx>
#include <rtl/ustrbuf.hxx>

#include "emfppen.hxx"
#include "emfpcustomlinecap.hxx"

using namespace ::com::sun::star;
using namespace ::basegfx;

namespace emfplushelper
{
    namespace {

    enum EmfPlusPenData
    {
        PenDataTransform        = 0x00000001,
        PenDataStartCap         = 0x00000002,
        PenDataEndCap           = 0x00000004,
        PenDataJoin             = 0x00000008,
        PenDataMiterLimit       = 0x00000010,
        PenDataLineStyle        = 0x00000020,
        PenDataDashedLineCap    = 0x00000040,
        PenDataDashedLineOffset = 0x00000080,
        PenDataDashedLine       = 0x00000100,
        PenDataAlignment        = 0x00000200,
        PenDataCompoundLine     = 0x00000400,
        PenDataCustomStartCap   = 0x00000800,
        PenDataCustomEndCap     = 0x00001000
    };

    }

    EMFPPen::EMFPPen()
        : penDataFlags(0)
        , penUnit(0)
        , penWidth(0.0)
        , startCap(0)
        , endCap(0)
        , lineJoin(0)
        , miterLimit(0.0)
        , dashStyle(0)
        , dashCap(0)
        , dashOffset(0.0)
        , alignment(0)
        , customStartCapLen(0)
        , customEndCapLen(0)
    {
    }

    EMFPPen::~EMFPPen()
    {
    }

    static OUString PenDataFlagsToString(sal_uInt32 flags)
    {
        rtl::OUStringBuffer sFlags;

        if (flags & EmfPlusPenDataTransform)
            sFlags.append("\nEMF+\t\t\tEmfPlusPenDataTransform");

        if (flags & EmfPlusPenDataStartCap)
            sFlags.append("\nEMF+\t\t\tEmfPlusPenDataStartCap");

        if (flags & EmfPlusPenDataEndCap)
             sFlags.append("\nEMF+\t\t\tEmfPlusPenDataEndCap");

        if (flags & EmfPlusPenDataJoin)
            sFlags.append("\nEMF+\t\t\tEmfPlusPenDataJoin");

        if (flags & EmfPlusPenDataMiterLimit)
            sFlags.append("\nEMF+\t\t\tEmfPlusPenDataMiterLimit");

        if (flags & EmfPlusPenDataLineStyle)
            sFlags.append("\nEMF+\t\t\tEmfPlusPenDataLineStyle");

        if (flags & EmfPlusPenDataDashedLineCap)
            sFlags.append("\nEMF+\t\t\tEmfPlusPenDataDashedLineCap");

        if (flags & EmfPlusPenDataDashedLineOffset)
            sFlags.append("\nEMF+\t\t\tEmfPlusPenDataDashedLineOffset");

        if (flags & EmfPlusPenDataDashedLine)
            sFlags.append("\nEMF+\t\t\tEmfPlusPenDataDashedLine");

        if (flags & EmfPlusPenDataAlignment)
            sFlags.append("\nEMF+\t\t\tEmfPlusPenDataAlignment");

        if (flags & EmfPlusPenDataCompoundLine)
            sFlags.append("\nEMF+\t\t\tEmfPlusPenDataCompoundLine");

        if (flags & EmfPlusPenDataCustomStartCap)
            sFlags.append("\nEMF+\t\t\tEmfPlusPenDataCustomStartCap");

        if (flags & EmfPlusPenDataCustomEndCap)
            sFlags.append("\nEMF+\t\t\tEmfPlusPenDataCustomEndCap");

        return sFlags.makeStringAndClear();
    }

    static OUString LineCapTypeToString(sal_uInt32 linecap)
    {
        switch (linecap)
        {
            case LineCapTypeFlat: return "LineCapTypeFlat";
            case LineCapTypeSquare: return "LineCapTypeSquare";
            case LineCapTypeRound: return "LineCapTypeRound";
            case LineCapTypeTriangle: return "LineCapTypeTriangle";
            case LineCapTypeNoAnchor: return "LineCapTypeNoAnchor";
            case LineCapTypeSquareAnchor: return "LineCapTypeSquareAnchor";
            case LineCapTypeRoundAnchor: return "LineCapTypeRoundAchor";
            case LineCapTypeDiamondAnchor: return "LineCapTypeDiamondAnchor";
            case LineCapTypeArrowAnchor: return "LineCapTypeArrowAnchor";
            case LineCapTypeAnchorMask: return "LineCapTypeAnchorMask";
            case LineCapTypeCustom: return "LineCapTypeCustom";
        }
        return "";
    }

    static OUString LineJoinTypeToString(sal_uInt32 jointype)
    {
        switch (jointype)
        {
            case LineJoinTypeMiter: return "LineJoinTypeMiter";
            case LineJoinTypeBevel: return "LineJoinTypeBevel";
            case LineJoinTypeRound: return "LineJoinTypeRound";
            case LineJoinTypeMiterClipped: return "LineJoinTypeMiterClipped";
        }
        return "";
    }

    static OUString DashedLineCapTypeToString(sal_uInt32 dashedlinecaptype)
    {
        switch (dashedlinecaptype)
        {
            case DashedLineCapTypeFlat: return "DashedLineCapTypeFlat";
            case DashedLineCapTypeRound: return "DashedLineCapTypeRound";
            case DashedLineCapTypeTriangle: return "DashedLineCapTypeTriangle";
        }
        return "";
    }

    static OUString PenAlignmentToString(sal_uInt32 alignment)
    {
        switch (alignment)
        {
            case PenAlignmentCenter: return "PenAlignmentCenter";
            case PenAlignmentInset: return "PenAlignmentInset";
            case PenAlignmentLeft: return "PenAlignmentLeft";
            case PenAlignmentOutset: return "PenAlignmentOutset";
            case PenAlignmentRight: return "PenAlignmentRight";
        }
        return "";
    }

    /// Convert stroke caps between EMF+ and rendering API
    sal_Int8 EMFPPen::lcl_convertStrokeCap(sal_uInt32 nEmfStroke)
    {
        switch (nEmfStroke)
        {
            case EmfPlusLineCapTypeSquare:
                return rendering::PathCapType::SQUARE;
            // we have no mapping for EmfPlusLineCapTypeTriangle,
            // but it is similar to Round
            case EmfPlusLineCapTypeTriangle: // fall-through
            case EmfPlusLineCapTypeRound:
                return rendering::PathCapType::ROUND;
        }

        return rendering::PathCapType::BUTT;
    }

    basegfx::B2DLineJoin EMFPPen::GetLineJoinType() const
    {
        if (penDataFlags & EmfPlusPenDataJoin) // additional line join information
        {
            switch (lineJoin)
            {
                case EmfPlusLineJoinTypeMiter: // fall-through
                case EmfPlusLineJoinTypeMiterClipped:
                    return basegfx::B2DLineJoin::Miter;
                case EmfPlusLineJoinTypeBevel:
                    return basegfx::B2DLineJoin::Bevel;
                case EmfPlusLineJoinTypeRound:
                    return basegfx::B2DLineJoin::Round;
            }
        }
        // If nothing set, then miter applied with no limit
        return basegfx::B2DLineJoin::Miter;
    }

    drawinglayer::attribute::StrokeAttribute
    EMFPPen::GetStrokeAttribute(const double aTransformation) const
    {
        if (penDataFlags & EmfPlusPenDataLineStyle // pen has a predefined line style
            && dashStyle != EmfPlusLineStyleCustom)
        {
            const double pw = aTransformation * penWidth;
            switch (dashStyle)
            {
                case EmfPlusLineStyleDash:
                    // [-loplugin:redundantfcast] false positive:
                    return drawinglayer::attribute::StrokeAttribute({ 3 * pw, pw });
                case EmfPlusLineStyleDot:
                    // [-loplugin:redundantfcast] false positive:
                    return drawinglayer::attribute::StrokeAttribute({ pw, pw });
                case EmfPlusLineStyleDashDot:
                    // [-loplugin:redundantfcast] false positive:
                    return drawinglayer::attribute::StrokeAttribute({ 3 * pw, pw, pw, pw });
                case EmfPlusLineStyleDashDotDot:
                    // [-loplugin:redundantfcast] false positive:
                    return drawinglayer::attribute::StrokeAttribute({ 3 * pw, pw, pw, pw, pw, pw });
            }
        }
        else if (penDataFlags & EmfPlusPenDataDashedLine) // pen has a custom dash line
        {
            const double pw = aTransformation * penWidth;
            // StrokeAttribute needs a double vector while the pen provides a float vector
            std::vector<double> aPattern(dashPattern.size());
            for (size_t i = 0; i < aPattern.size(); i++)
            {
                // convert from float to double and multiply with the adjusted pen width
                aPattern[i] = pw * dashPattern[i];
            }
            return drawinglayer::attribute::StrokeAttribute(std::move(aPattern));
        }
        //  EmfPlusLineStyleSolid: - do nothing special, use default stroke attribute
        return drawinglayer::attribute::StrokeAttribute();
    }

    void EMFPPen::Read(SvStream& s, EmfPlusHelperData const & rR)
    {
        sal_uInt32 graphicsVersion, penType;
        s.ReadUInt32(graphicsVersion).ReadUInt32(penType).ReadUInt32(penDataFlags).ReadUInt32(penUnit).ReadFloat(penWidth);
        SAL_INFO("drawinglayer.emf", "EMF+\t\tGraphics version: 0x" << std::hex << graphicsVersion);
        SAL_INFO("drawinglayer.emf", "EMF+\t\tType: " << penType);
        SAL_INFO("drawinglayer.emf", "EMF+\t\tPen data flags: 0x" << penDataFlags << PenDataFlagsToString(penDataFlags));
        SAL_INFO("drawinglayer.emf", "EMF+\t\tUnit: " << UnitTypeToString(penUnit));
        SAL_INFO("drawinglayer.emf", "EMF+\t\tWidth: " << std::dec << penWidth);

        // If a zero width is specified, a minimum value must be used, which is determined by the units
        if (penWidth == 0.0)
        { //TODO Check if these values is correct
            penWidth = penUnit == 0 ? 0.18f
                : 0.05f;  // 0.05f is taken from old EMF+ implementation (case of Unit == Pixel etc.)
        }

        if (penDataFlags & PenDataTransform)
        {
            EmfPlusHelperData::readXForm(s, pen_transformation);
            SAL_WARN("drawinglayer.emf", "EMF+\t\t TODO PenDataTransform: " << pen_transformation);
        }

        if (penDataFlags & PenDataStartCap)
        {
            s.ReadInt32(startCap);
            SAL_INFO("drawinglayer.emf", "EMF+\t\tstartCap: " << LineCapTypeToString(startCap) << " (0x" << std::hex << startCap << ")");
        }
        else
        {
            startCap = 0;
        }

        if (penDataFlags & PenDataEndCap)
        {
            s.ReadInt32(endCap);
            SAL_INFO("drawinglayer.emf", "EMF+\t\tendCap: " << LineCapTypeToString(endCap) << " (0x" << std::hex << startCap << ")");
        }
        else
        {
            endCap = 0;
        }

        if (penDataFlags & PenDataJoin)
        {
            s.ReadInt32(lineJoin);
            SAL_WARN("drawinglayer.emf", "EMF+\t\t LineJoin: " << LineJoinTypeToString(lineJoin) << " (0x" << std::hex << lineJoin << ")");
        }
        else
        {
            lineJoin = 0;
        }

        if (penDataFlags & PenDataMiterLimit)
        {
            s.ReadFloat(miterLimit);
            SAL_WARN("drawinglayer.emf", "EMF+\t\tTODO PenDataMiterLimit: " << std::dec << miterLimit);
        }
        else
        {
            miterLimit = 0;
        }

        if (penDataFlags & PenDataLineStyle)
        {
            s.ReadInt32(dashStyle);
            SAL_INFO("drawinglayer.emf", "EMF+\t\tdashStyle: " << DashedLineCapTypeToString(dashStyle) << " (0x" << std::hex << dashStyle << ")");
        }
        else
        {
            dashStyle = 0;
        }

        if (penDataFlags & PenDataDashedLineCap)
        {
            s.ReadInt32(dashCap);
            SAL_WARN("drawinglayer.emf", "EMF+\t\t TODO PenDataDashedLineCap: 0x" << std::hex << dashCap);
        }
        else
        {
            dashCap = 0;
        }

        if (penDataFlags & PenDataDashedLineOffset)
        {
            s.ReadFloat(dashOffset);
            SAL_WARN("drawinglayer.emf", "EMF+\t\t TODO PenDataDashedLineOffset: 0x" << std::hex << dashOffset);
        }
        else
        {
            dashOffset = 0;
        }

        if (penDataFlags & PenDataDashedLine)
        {
            dashStyle = EmfPlusLineStyleCustom;
            sal_uInt32 dashPatternLen;

            s.ReadUInt32(dashPatternLen);
            SAL_INFO("drawinglayer.emf", "EMF+\t\t\tdashPatternLen: " << dashPatternLen);

            dashPattern.resize( dashPatternLen );

            for (sal_uInt32 i = 0; i < dashPatternLen; i++)
            {
                s.ReadFloat(dashPattern[i]);
                SAL_INFO("drawinglayer.emf", "EMF+\t\t\t\tdashPattern[" << i << "]: " << dashPattern[i]);
            }
        }

        if (penDataFlags & PenDataAlignment)
        {
            s.ReadInt32(alignment);
            SAL_WARN("drawinglayer.emf", "EMF+\t\t\tTODO PenDataAlignment: " << PenAlignmentToString(alignment) << " (0x" << std::hex << alignment << ")");
        }
        else
        {
            alignment = 0;
        }

        if (penDataFlags & PenDataCompoundLine)
        {
            SAL_WARN("drawinglayer.emf", "EMF+\t\t\tTODO PenDataCompoundLine");
            sal_uInt32 compoundArrayLen;
            s.ReadUInt32(compoundArrayLen);

            compoundArray.resize(compoundArrayLen);

            for (sal_uInt32 i = 0; i < compoundArrayLen; i++)
            {
                s.ReadFloat(compoundArray[i]);
                SAL_INFO("drawinglayer.emf", "EMF+\t\t\t\tcompoundArray[" << i << "]: " << compoundArray[i]);
            }
        }

        if (penDataFlags & PenDataCustomStartCap)
        {
            s.ReadUInt32(customStartCapLen);
            SAL_INFO("drawinglayer.emf", "EMF+\t\t\tcustomStartCapLen: " << customStartCapLen);
            sal_uInt64 const pos = s.Tell();

            customStartCap.reset( new EMFPCustomLineCap() );
            customStartCap->Read(s, rR);

            // maybe we don't read everything yet, play it safe ;-)
            s.Seek(pos + customStartCapLen);
        }
        else
        {
            customStartCapLen = 0;
        }

        if (penDataFlags & PenDataCustomEndCap)
        {
            s.ReadUInt32(customEndCapLen);
            SAL_INFO("drawinglayer.emf", "EMF+\t\t\tcustomEndCapLen: " << customEndCapLen);
            sal_uInt64 const pos = s.Tell();

            customEndCap.reset( new EMFPCustomLineCap() );
            customEndCap->Read(s, rR);

            // maybe we don't read everything yet, play it safe ;-)
            s.Seek(pos + customEndCapLen);
        }
        else
        {
            customEndCapLen = 0;
        }

        EMFPBrush::Read(s, rR);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
