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

#include <richstring.hxx>
#include <biffhelper.hxx>

#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/text/XText.hpp>
#include <rtl/ustrbuf.hxx>
#include <editeng/editobj.hxx>
#include <osl/diagnose.h>
#include <oox/helper/binaryinputstream.hxx>
#include <oox/helper/attributelist.hxx>
#include <oox/helper/propertyset.hxx>
#include <oox/token/tokens.hxx>
#include <editutil.hxx>

#include <vcl/svapp.hxx>

namespace oox::xls {

using namespace ::com::sun::star::text;
using namespace ::com::sun::star::uno;

namespace {

const sal_uInt8 BIFF12_STRINGFLAG_FONTS         = 0x01;
const sal_uInt8 BIFF12_STRINGFLAG_PHONETICS     = 0x02;

bool lclNeedsRichTextFormat( const oox::xls::Font* pFont )
{
    return pFont && pFont->needsRichTextFormat();
}

sal_Int32 lcl_getHexLetterValue(sal_Unicode nCode)
{
    if (nCode >= '0' && nCode <= '9')
        return nCode - '0';

    if (nCode >= 'A' && nCode <= 'F')
        return nCode - 'A' + 10;

    if (nCode >= 'a' && nCode <= 'f')
        return nCode - 'a' + 10;

    return -1;
}

bool lcl_validEscape(sal_Unicode nCode)
{
    // Valid XML chars that can be escaped (ignoring the restrictions) as in the OOX open spec
    // 2.1.1742 Part 1 Section 22.9.2.19, ST_Xstring (Escaped String)
    if (nCode == 0x000D || nCode == 0x000A || nCode == 0x0009 || nCode == 0x005F)
        return true;

    // Other valid XML chars in basic multilingual plane that cannot be escaped.
    if ((nCode >= 0x0020 && nCode <= 0xD7FF) || (nCode >= 0xE000 && nCode <= 0xFFFD))
        return false;

    return true;
}

OUString lcl_unEscapeUnicodeChars(const OUString& rSrc)
{
    // Example: Escaped representation of unicode char 0x000D is _x000D_

    sal_Int32 nLen = rSrc.getLength();
    if (!nLen)
        return rSrc;

    sal_Int32 nStart = 0;
    bool bFound = false;
    const OUString aPrefix = "_x";
    sal_Int32 nPrefixStart = rSrc.indexOf(aPrefix, nStart);

    if (nPrefixStart == -1)
        return rSrc;

    OUStringBuffer aBuf(rSrc);
    sal_Int32 nOffset = 0; // index offset in aBuf w.r.t rSrc.

    do
    {
        sal_Int32 nEnd = -1;
        sal_Unicode nCode = 0;
        bool bFoundThis = false;
        for (sal_Int32 nIdx = 0; nIdx < 5; ++nIdx)
        {
            sal_Int32 nThisIdx = nPrefixStart + nIdx + 2;
            if (nThisIdx >= nLen)
                break;

            sal_Unicode nThisCode = rSrc[nThisIdx];
            sal_Int32 nLetter = lcl_getHexLetterValue(nThisCode);

            if (!nIdx && nLetter < 0)
                break;

            if (nLetter >= 0)
            {
                nCode = (nCode << 4) + static_cast<sal_Unicode>(nLetter);
            }
            else if (nThisCode == '_')
            {
                nEnd = nThisIdx + 1;
                bFoundThis = true;
                break;
            }
            else
            {
                break;
            }
        }

        if (bFoundThis)
        {
            // nEnd is already set inside the inner loop in this case.
            if (lcl_validEscape(nCode))
            {
                bFound = true;
                sal_Int32 nEscStrLen = nEnd - nPrefixStart;
                aBuf.remove(nPrefixStart - nOffset, nEscStrLen);
                aBuf.insert(nPrefixStart - nOffset, nCode);

                nOffset += nEscStrLen - 1;
            }
        }
        else
        {
            // Start the next search just after last "_x"
            nEnd = nPrefixStart + 2;
        }

        nStart = nEnd;
        nPrefixStart = rSrc.indexOf(aPrefix, nStart);
    }
    while (nPrefixStart != -1);

    if (bFound)
        return aBuf.makeStringAndClear();

    return rSrc;
}

} // namespace

RichStringPortion::RichStringPortion( const WorkbookHelper& rHelper ) :
    WorkbookHelper( rHelper ),
    mnFontId( -1 ),
    mbConverted( false )
{
}

void RichStringPortion::setText( const OUString& rText )
{
    maText = lcl_unEscapeUnicodeChars(rText);
}

FontRef const & RichStringPortion::createFont()
{
    mxFont = std::make_shared<Font>( *this, false );
    return mxFont;
}

void RichStringPortion::setFontId( sal_Int32 nFontId )
{
    mnFontId = nFontId;
}

void RichStringPortion::finalizeImport()
{
    if( mxFont )
        mxFont->finalizeImport();
    else if( mnFontId >= 0 )
        mxFont = getStyles().getFont( mnFontId );
}

void RichStringPortion::convert( const Reference< XText >& rxText, bool bReplace )
{
    if ( mbConverted )
        return;

    Reference< XTextRange > xRange;
    if( bReplace )
        xRange = rxText;
    else
        xRange = rxText->getEnd();
    OSL_ENSURE( xRange.is(), "RichStringPortion::convert - cannot get text range interface" );

    if( xRange.is() )
    {
        xRange->setString( maText );
        if( mxFont )
        {
            PropertySet aPropSet( xRange );
            mxFont->writeToPropertySet( aPropSet );
        }
    }

    mbConverted = true;
}

void RichStringPortion::convert( ScEditEngineDefaulter& rEE, ESelection& rSelection, const oox::xls::Font* pFont )
{
    rSelection.nStartPos = rSelection.nEndPos;
    rSelection.nStartPara = rSelection.nEndPara;
    SfxItemSet aItemSet( rEE.GetEmptyItemSet() );

    const Font* pFontToUse = mxFont ? mxFont.get() : lclNeedsRichTextFormat( pFont ) ? pFont : nullptr;

    if ( pFontToUse )
        pFontToUse->fillToItemSet( aItemSet, true );

    // #TODO need to manually adjust nEndPos ( and nEndPara ) to cater for any paragraphs
    sal_Int32 nLastParaLoc = -1;
    sal_Int32 nSearchIndex = maText.indexOf( '\n' );
    sal_Int32 nParaOccurrence = 0;
    while ( nSearchIndex != -1 )
    {
        nLastParaLoc = nSearchIndex;
        ++nParaOccurrence;
        rSelection.nEndPos = 0;
        nSearchIndex = maText.indexOf( '\n', nSearchIndex + 1);
    }

    rSelection.nEndPara += nParaOccurrence;
    if ( nLastParaLoc != -1 )
    {
        rSelection.nEndPos = maText.getLength() - 1 - nLastParaLoc;
    }
    else
    {
        rSelection.nEndPos = rSelection.nStartPos + maText.getLength();
    }
    rEE.QuickSetAttribs( aItemSet, rSelection );
}

void RichStringPortion::writeFontProperties( const Reference<XText>& rxText ) const
{
    PropertySet aPropSet(rxText);

    if (mxFont)
        mxFont->writeToPropertySet(aPropSet);
}

void FontPortionModel::read( SequenceInputStream& rStrm )
{
    mnPos = rStrm.readuInt16();
    mnFontId = rStrm.readuInt16();
}

void FontPortionModelList::appendPortion( const FontPortionModel& rPortion )
{
    // #i33341# real life -- same character index may occur several times
    OSL_ENSURE( mvModels.empty() || (mvModels.back().mnPos <= rPortion.mnPos), "FontPortionModelList::appendPortion - wrong char order" );
    if( mvModels.empty() || (mvModels.back().mnPos < rPortion.mnPos) )
        mvModels.push_back( rPortion );
    else
        mvModels.back().mnFontId = rPortion.mnFontId;
}

void FontPortionModelList::importPortions( SequenceInputStream& rStrm )
{
    sal_Int32 nCount = rStrm.readInt32();
    mvModels.clear();
    if( nCount > 0 )
    {
        mvModels.reserve( getLimitedValue< size_t, sal_Int64 >( nCount, 0, rStrm.getRemaining() / 4 ) );
        /*  #i33341# real life -- same character index may occur several times
            -> use appendPortion() to validate string position. */
        FontPortionModel aPortion;
        for( sal_Int32 nIndex = 0; !rStrm.isEof() && (nIndex < nCount); ++nIndex )
        {
            aPortion.read( rStrm );
            appendPortion( aPortion );
        }
    }
}

PhoneticDataModel::PhoneticDataModel() :
    mnFontId( -1 ),
    mnType( XML_fullwidthKatakana ),
    mnAlignment( XML_left )
{
}

void PhoneticDataModel::setBiffData( sal_Int32 nType, sal_Int32 nAlignment )
{
    static const sal_Int32 spnTypeIds[] = { XML_halfwidthKatakana, XML_fullwidthKatakana, XML_hiragana, XML_noConversion };
    mnType = STATIC_ARRAY_SELECT( spnTypeIds, nType, XML_fullwidthKatakana );

    static const sal_Int32 spnAlignments[] = { XML_noControl, XML_left, XML_center, XML_distributed };
    mnAlignment = STATIC_ARRAY_SELECT( spnAlignments, nAlignment, XML_left );
}

PhoneticSettings::PhoneticSettings( const WorkbookHelper& rHelper ) :
    WorkbookHelper( rHelper )
{
}

void PhoneticSettings::importPhoneticPr( const AttributeList& rAttribs )
{
    maModel.mnFontId    = rAttribs.getInteger( XML_fontId, -1 );
    maModel.mnType      = rAttribs.getToken( XML_type, XML_fullwidthKatakana );
    maModel.mnAlignment = rAttribs.getToken( XML_alignment, XML_left );
}

void PhoneticSettings::importPhoneticPr( SequenceInputStream& rStrm )
{
    sal_uInt16 nFontId;
    sal_Int32 nType, nAlignment;
    nFontId = rStrm.readuInt16();
    nType = rStrm.readInt32();
    nAlignment = rStrm.readInt32();
    maModel.mnFontId = nFontId;
    maModel.setBiffData( nType, nAlignment );
}

void PhoneticSettings::importStringData( SequenceInputStream& rStrm )
{
    sal_uInt16 nFontId, nFlags;
    nFontId = rStrm.readuInt16();
    nFlags = rStrm.readuInt16();
    maModel.mnFontId = nFontId;
    maModel.setBiffData( extractValue< sal_Int32 >( nFlags, 0, 2 ), extractValue< sal_Int32 >( nFlags, 2, 2 ) );
}

RichStringPhonetic::RichStringPhonetic( const WorkbookHelper& rHelper ) :
    WorkbookHelper( rHelper ),
    mnBasePos( -1 ),
    mnBaseEnd( -1 )
{
}

void RichStringPhonetic::setText( const OUString& rText )
{
    maText = rText;
}

void RichStringPhonetic::importPhoneticRun( const AttributeList& rAttribs )
{
    mnBasePos = rAttribs.getInteger( XML_sb, -1 );
    mnBaseEnd = rAttribs.getInteger( XML_eb, -1 );
}

void RichStringPhonetic::setBaseRange( sal_Int32 nBasePos, sal_Int32 nBaseEnd )
{
    mnBasePos = nBasePos;
    mnBaseEnd = nBaseEnd;
}

void PhoneticPortionModel::read( SequenceInputStream& rStrm )
{
    mnPos = rStrm.readuInt16();
    mnBasePos = rStrm.readuInt16();
    mnBaseLen = rStrm.readuInt16();
}

void PhoneticPortionModelList::appendPortion( const PhoneticPortionModel& rPortion )
{
    // same character index may occur several times
    OSL_ENSURE( mvModels.empty() || ((mvModels.back().mnPos <= rPortion.mnPos) &&
        (mvModels.back().mnBasePos + mvModels.back().mnBaseLen <= rPortion.mnBasePos)),
        "PhoneticPortionModelList::appendPortion - wrong char order" );
    if( mvModels.empty() || (mvModels.back().mnPos < rPortion.mnPos) )
    {
        mvModels.push_back( rPortion );
    }
    else if( mvModels.back().mnPos == rPortion.mnPos )
    {
        mvModels.back().mnBasePos = rPortion.mnBasePos;
        mvModels.back().mnBaseLen = rPortion.mnBaseLen;
    }
}

void PhoneticPortionModelList::importPortions( SequenceInputStream& rStrm )
{
    sal_Int32 nCount = rStrm.readInt32();
    mvModels.clear();
    if( nCount > 0 )
    {
        mvModels.reserve( getLimitedValue< size_t, sal_Int64 >( nCount, 0, rStrm.getRemaining() / 6 ) );
        PhoneticPortionModel aPortion;
        for( sal_Int32 nIndex = 0; !rStrm.isEof() && (nIndex < nCount); ++nIndex )
        {
            aPortion.read( rStrm );
            appendPortion( aPortion );
        }
    }
}

RichString::RichString( const WorkbookHelper& rHelper ) :
    WorkbookHelper( rHelper ),
    maPhonSettings( rHelper )
{
}

RichStringPortionRef RichString::importText()
{
    return createPortion();
}

RichStringPortionRef RichString::importRun()
{
    return createPortion();
}

RichStringPhoneticRef RichString::importPhoneticRun( const AttributeList& rAttribs )
{
    RichStringPhoneticRef xPhonetic = createPhonetic();
    xPhonetic->importPhoneticRun( rAttribs );
    return xPhonetic;
}

void RichString::importPhoneticPr( const AttributeList& rAttribs )
{
    maPhonSettings.importPhoneticPr( rAttribs );
}

void RichString::importString( SequenceInputStream& rStrm, bool bRich )
{
    sal_uInt8 nFlags = bRich ? rStrm.readuInt8() : 0;
    OUString aBaseText = BiffHelper::readString( rStrm );

    if( !rStrm.isEof() && getFlag( nFlags, BIFF12_STRINGFLAG_FONTS ) )
    {
        FontPortionModelList aPortions;
        aPortions.importPortions( rStrm );
        createTextPortions( aBaseText, aPortions );
    }
    else
    {
        createPortion()->setText( aBaseText );
    }

    if( !rStrm.isEof() && getFlag( nFlags, BIFF12_STRINGFLAG_PHONETICS ) )
    {
        OUString aPhoneticText = BiffHelper::readString( rStrm );
        PhoneticPortionModelList aPortions;
        aPortions.importPortions( rStrm );
        maPhonSettings.importStringData( rStrm );
        createPhoneticPortions( aPhoneticText, aPortions, aBaseText.getLength() );
    }
}

void RichString::finalizeImport()
{
    maTextPortions.forEachMem( &RichStringPortion::finalizeImport );
}

bool RichString::extractPlainString( OUString& orString, const oox::xls::Font* pFirstPortionFont ) const
{
    if( !maPhonPortions.empty() )
        return false;
    if( maTextPortions.empty() )
    {
        orString.clear();
        return true;
    }
    if( (maTextPortions.size() == 1) && !maTextPortions.front()->hasFont() && !lclNeedsRichTextFormat( pFirstPortionFont ) )
    {
        orString = maTextPortions.front()->getText();
        return orString.indexOf( '\x0A' ) < 0;
    }
    return false;
}

void RichString::convert( const Reference< XText >& rxText ) const
{
    if (maTextPortions.size() == 1)
    {
        // Set text directly to the cell when the string has only one portion.
        // It's much faster this way.
        RichStringPortion& rPtn = *maTextPortions.front();
        rxText->setString(rPtn.getText());
        rPtn.writeFontProperties(rxText);
        return;
    }

    bool bReplaceOld = true;
    for( const auto& rxTextPortion : maTextPortions )
    {
        rxTextPortion->convert( rxText, bReplaceOld );
        bReplaceOld = false;    // do not replace first portion text with following portions
    }
}

std::unique_ptr<EditTextObject> RichString::convert( ScEditEngineDefaulter& rEE, const oox::xls::Font* pFirstPortionFont ) const
{
    ESelection aSelection;

    OUStringBuffer sString;
    for( const auto& rxTextPortion : maTextPortions )
        sString.append(rxTextPortion->getText());

    // fdo#84370 - diving into editeng is not thread safe.
    SolarMutexGuard aGuard;

    rEE.SetTextCurrentDefaults( sString.makeStringAndClear() );

    for( const auto& rxTextPortion : maTextPortions )
    {
        rxTextPortion->convert( rEE, aSelection, pFirstPortionFont );
        pFirstPortionFont = nullptr;
    }

    return rEE.CreateTextObject();
}

// private --------------------------------------------------------------------

RichStringPortionRef RichString::createPortion()
{
    RichStringPortionRef xPortion = std::make_shared<RichStringPortion>( *this );
    maTextPortions.push_back( xPortion );
    return xPortion;
}

RichStringPhoneticRef RichString::createPhonetic()
{
    RichStringPhoneticRef xPhonetic = std::make_shared<RichStringPhonetic>( *this );
    maPhonPortions.push_back( xPhonetic );
    return xPhonetic;
}

void RichString::createTextPortions( const OUString& rText, FontPortionModelList& rPortions )
{
    maTextPortions.clear();
    if( rText.isEmpty() )
        return;

    sal_Int32 nStrLen = rText.getLength();
    // add leading and trailing string position to ease the following loop
    if( rPortions.empty() || (rPortions.front().mnPos > 0) )
        rPortions.insert( rPortions.begin(), FontPortionModel( 0 ) );
    if( rPortions.back().mnPos < nStrLen )
        rPortions.push_back( FontPortionModel( nStrLen ) );

    // create all string portions according to the font id vector
    for( ::std::vector< FontPortionModel >::const_iterator aIt = rPortions.begin(); aIt->mnPos < nStrLen; ++aIt )
    {
        sal_Int32 nPortionLen = (aIt + 1)->mnPos - aIt->mnPos;
        if( (0 < nPortionLen) && (aIt->mnPos + nPortionLen <= nStrLen) )
        {
            RichStringPortionRef xPortion = createPortion();
            xPortion->setText( rText.copy( aIt->mnPos, nPortionLen ) );
            xPortion->setFontId( aIt->mnFontId );
        }
    }
}

void RichString::createPhoneticPortions( const OUString& rText, PhoneticPortionModelList& rPortions, sal_Int32 nBaseLen )
{
    maPhonPortions.clear();
    if( rText.isEmpty())
        return;

    sal_Int32 nStrLen = rText.getLength();
    // no portions - assign phonetic text to entire base text
    if( rPortions.empty() )
        rPortions.push_back( PhoneticPortionModel( 0, 0, nBaseLen ) );
    // add trailing string position to ease the following loop
    if( rPortions.back().mnPos < nStrLen )
        rPortions.push_back( PhoneticPortionModel( nStrLen, nBaseLen, 0 ) );

    // create all phonetic portions according to the portions vector
    for( ::std::vector< PhoneticPortionModel >::const_iterator aIt = rPortions.begin(); aIt->mnPos < nStrLen; ++aIt )
    {
        sal_Int32 nPortionLen = (aIt + 1)->mnPos - aIt->mnPos;
        if( (0 < nPortionLen) && (aIt->mnPos + nPortionLen <= nStrLen) )
        {
            RichStringPhoneticRef xPhonetic = createPhonetic();
            xPhonetic->setText( rText.copy( aIt->mnPos, nPortionLen ) );
            xPhonetic->setBaseRange( aIt->mnBasePos, aIt->mnBasePos + aIt->mnBaseLen );
        }
    }
}

} // namespace oox::xls

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
