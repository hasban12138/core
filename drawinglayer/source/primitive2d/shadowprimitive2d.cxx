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

#include <drawinglayer/primitive2d/shadowprimitive2d.hxx>
#include <basegfx/color/bcolormodifier.hxx>
#include <drawinglayer/primitive2d/modifiedcolorprimitive2d.hxx>
#include <drawinglayer/primitive2d/transformprimitive2d.hxx>
#include <drawinglayer/primitive2d/drawinglayer_primitivetypes2d.hxx>

#include <memory>

using namespace com::sun::star;


namespace drawinglayer::primitive2d
{
        ShadowPrimitive2D::ShadowPrimitive2D(
            const basegfx::B2DHomMatrix& rShadowTransform,
            const basegfx::BColor& rShadowColor,
            double fShadowBlur,
            Primitive2DContainer&& aChildren)
        :   GroupPrimitive2D(std::move(aChildren)),
            maShadowTransform(rShadowTransform),
            maShadowColor(rShadowColor),
            mfShadowBlur(fShadowBlur)
        {
        }

        bool ShadowPrimitive2D::operator==(const BasePrimitive2D& rPrimitive) const
        {
            if(BasePrimitive2D::operator==(rPrimitive))
            {
                const ShadowPrimitive2D& rCompare = static_cast< const ShadowPrimitive2D& >(rPrimitive);

                return (getShadowTransform() == rCompare.getShadowTransform()
                    && getShadowColor() == rCompare.getShadowColor()
                    && getShadowBlur() == rCompare.getShadowBlur());
            }

            return false;
        }

        basegfx::B2DRange ShadowPrimitive2D::getB2DRange(const geometry::ViewInformation2D& rViewInformation) const
        {
            basegfx::B2DRange aRetval(getChildren().getB2DRange(rViewInformation));
            aRetval.grow(getShadowBlur());
            aRetval.transform(getShadowTransform());
            return aRetval;
        }

        void ShadowPrimitive2D::get2DDecomposition(Primitive2DDecompositionVisitor& rVisitor, const geometry::ViewInformation2D& /*rViewInformation*/) const
        {
            if(getChildren().empty())
                return;

            // create a modifiedColorPrimitive containing the shadow color and the content
            const basegfx::BColorModifierSharedPtr aBColorModifier =
                std::make_shared<basegfx::BColorModifier_replace>(
                    getShadowColor());
            const Primitive2DReference xRefA(
                new ModifiedColorPrimitive2D(
                    Primitive2DContainer(getChildren()),
                    aBColorModifier));
            Primitive2DContainer aSequenceB { xRefA };

            // build transformed primitiveVector with shadow offset and add to target
            rVisitor.visit(new TransformPrimitive2D(getShadowTransform(), std::move(aSequenceB)));
        }

        // provide unique ID
        sal_uInt32 ShadowPrimitive2D::getPrimitive2DID() const
        {
            return PRIMITIVE2D_ID_SHADOWPRIMITIVE2D;
        }

} // end of namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
