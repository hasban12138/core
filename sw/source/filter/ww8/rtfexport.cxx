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

#include <sal/config.h>

#include "rtfexport.hxx"

#include <string_view>

#include "rtfexportfilter.hxx"
#include "rtfsdrexport.hxx"
#include "rtfattributeoutput.hxx"
#include <com/sun/star/document/XDocumentPropertiesSupplier.hpp>
#include <com/sun/star/frame/XModel.hpp>
#include <com/sun/star/i18n/ScriptType.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/beans/XPropertySetInfo.hpp>
#include <docsh.hxx>
#include <viewsh.hxx>
#include <viewopt.hxx>
#include <fmtpdsc.hxx>
#include <ftninfo.hxx>
#include <fmthdft.hxx>
#include <editeng/colritem.hxx>
#include <editeng/udlnitem.hxx>
#include <editeng/fontitem.hxx>
#include <editeng/paperinf.hxx>
#include <editeng/brushitem.hxx>
#include <editeng/protitem.hxx>
#include <editeng/lrspitem.hxx>
#include <editeng/ulspitem.hxx>
#include <editeng/boxitem.hxx>
#include <editeng/shaditem.hxx>
#include <lineinfo.hxx>
#include <redline.hxx>
#include <swmodule.hxx>
#include <IDocumentLayoutAccess.hxx>
#include <comphelper/string.hxx>
#include <svtools/rtfkeywd.hxx>
#include <filter/msfilter/rtfutil.hxx>
#include <unotools/docinfohelper.hxx>
#include <o3tl/string_view.hxx>
#include <osl/diagnose.h>
#include <rtl/tencinfo.h>
#include <sal/log.hxx>
#include <svx/xflclit.hxx>
#include <fmtmeta.hxx>
#include <IDocumentSettingAccess.hxx>
#include <fmtfsize.hxx>
#include <ndtxt.hxx>
#include <numrule.hxx>
#include <frmatr.hxx>
#include <swtable.hxx>
#include <IMark.hxx>

using namespace ::com::sun::star;

// the default text encoding for the export, if it doesn't fit unicode will
// be used
#define DEF_ENCODING RTL_TEXTENCODING_ASCII_US

AttributeOutputBase& RtfExport::AttrOutput() const { return *m_pAttrOutput; }

MSWordSections& RtfExport::Sections() const { return *m_pSections; }

RtfSdrExport& RtfExport::SdrExporter() const { return *m_pSdrExport; }

bool RtfExport::CollapseScriptsforWordOk(sal_uInt16 nScript, sal_uInt16 nWhich)
{
    // FIXME is this actually true for rtf? - this is copied from DOCX
    if (nScript == i18n::ScriptType::ASIAN)
    {
        // for asian in ww8, there is only one fontsize
        // and one fontstyle (posture/weight)
        switch (nWhich)
        {
            case RES_CHRATR_FONTSIZE:
            case RES_CHRATR_POSTURE:
            case RES_CHRATR_WEIGHT:
                return false;
            default:
                break;
        }
    }
    else if (nScript != i18n::ScriptType::COMPLEX)
    {
        // for western in ww8, there is only one fontsize
        // and one fontstyle (posture/weight)
        switch (nWhich)
        {
            case RES_CHRATR_CJK_FONTSIZE:
            case RES_CHRATR_CJK_POSTURE:
            case RES_CHRATR_CJK_WEIGHT:
                return false;
            default:
                break;
        }
    }
    return true;
}

void RtfExport::AppendBookmarks(const SwTextNode& rNode, sal_Int32 nCurrentPos, sal_Int32 nLen,
                                const SwRedlineData* /*pRedlineData*/)
{
    std::vector<OUString> aStarts;
    std::vector<OUString> aEnds;

    IMarkVector aMarks;
    if (GetBookmarks(rNode, nCurrentPos, nCurrentPos + nLen, aMarks))
    {
        for (const auto& pMark : aMarks)
        {
            const sal_Int32 nStart = pMark->GetMarkStart().nContent.GetIndex();
            const sal_Int32 nEnd = pMark->GetMarkEnd().nContent.GetIndex();

            if (nStart == nCurrentPos)
                aStarts.push_back(pMark->GetName());

            if (nEnd == nCurrentPos)
                aEnds.push_back(pMark->GetName());
        }
    }

    m_pAttrOutput->WriteBookmarks_Impl(aStarts, aEnds);
}

void RtfExport::AppendBookmark(const OUString& rName)
{
    std::vector<OUString> aStarts{ rName };
    std::vector<OUString> aEnds{ rName };

    m_pAttrOutput->WriteBookmarks_Impl(aStarts, aEnds);
}

void RtfExport::AppendAnnotationMarks(const SwWW8AttrIter& rAttrs, sal_Int32 nCurrentPos,
                                      sal_Int32 nLen)
{
    std::vector<OUString> aStarts;
    std::vector<OUString> aEnds;

    IMarkVector aMarks;
    if (GetAnnotationMarks(rAttrs, nCurrentPos, nCurrentPos + nLen, aMarks))
    {
        for (const auto& pMark : aMarks)
        {
            const sal_Int32 nStart = pMark->GetMarkStart().nContent.GetIndex();
            const sal_Int32 nEnd = pMark->GetMarkEnd().nContent.GetIndex();

            if (nStart == nCurrentPos)
                aStarts.push_back(pMark->GetName());

            if (nEnd == nCurrentPos)
                aEnds.push_back(pMark->GetName());
        }
    }

    m_pAttrOutput->WriteAnnotationMarks_Impl(aStarts, aEnds);
}

//For i120928,to export graphic of bullet for RTF filter
void RtfExport::ExportGrfBullet(const SwTextNode& /*rNd*/)
{
    // Noop, would be too late, see WriteNumbering() instead.
}

void RtfExport::WriteChar(sal_Unicode /*c*/) { /* WriteChar() has nothing to do for rtf. */}

static bool IsExportNumRule(const SwNumRule& rRule)
{
    sal_uInt8 nEnd = MAXLEVEL;
    while (nEnd-- && !rRule.GetNumFormat(nEnd))
        ;
    ++nEnd;

    sal_uInt8 nLvl;

    for (nLvl = 0; nLvl < nEnd; ++nLvl)
    {
        const SwNumFormat* pNFormat = &rRule.Get(nLvl);
        if (SVX_NUM_NUMBER_NONE != pNFormat->GetNumberingType() || !pNFormat->GetPrefix().isEmpty()
            || (!pNFormat->GetSuffix().isEmpty() && pNFormat->GetSuffix() != "."))
            break;
    }

    return nLvl != nEnd;
}

void RtfExport::BuildNumbering()
{
    const SwNumRuleTable& rListTable = m_rDoc.GetNumRuleTable();

    SwNumRule* pOutlineRule = m_rDoc.GetOutlineNumRule();
    if (IsExportNumRule(*pOutlineRule))
        GetNumberingId(*pOutlineRule);

    for (auto n = rListTable.size(); n;)
    {
        SwNumRule* pRule = rListTable[--n];
        if (!m_rDoc.IsUsed(*pRule))
            continue;

        if (IsExportNumRule(*pRule))
            GetNumberingId(*pRule);
    }
}

void RtfExport::WriteNumbering()
{
    SAL_INFO("sw.rtf", __func__ << " start");

    if (!m_pUsedNumTable)
        return; // no numbering is used

    Strm()
        .WriteChar('{')
        .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_IGNORE)
        .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_LISTTABLE);

    CollectGrfsOfBullets();
    if (!m_vecBulletPic.empty())
        Strm()
            .WriteChar('{')
            .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_IGNORE)
            .WriteCharPtr(LO_STRING_SVTOOLS_RTF_LISTPICTURE);
    BulletDefinitions();
    if (!m_vecBulletPic.empty())
        Strm().WriteChar('}');

    AbstractNumberingDefinitions();
    Strm().WriteChar('}');

    Strm().WriteChar('{').WriteCharPtr(OOO_STRING_SVTOOLS_RTF_LISTOVERRIDETABLE);
    NumberingDefinitions();
    Strm().WriteChar('}');

    SAL_INFO("sw.rtf", __func__ << " end");
}

void RtfExport::WriteRevTab()
{
    int nRevAuthors = m_rDoc.getIDocumentRedlineAccess().GetRedlineTable().size();

    if (nRevAuthors < 1)
        return;

    // RTF always seems to use Unknown as the default first entry
    GetRedline("Unknown");

    for (SwRangeRedline* pRedl : m_rDoc.getIDocumentRedlineAccess().GetRedlineTable())
    {
        GetRedline(SW_MOD()->GetRedlineAuthor(pRedl->GetAuthor()));
    }

    // Now write the table
    Strm()
        .WriteChar('{')
        .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_IGNORE)
        .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_REVTBL)
        .WriteChar(' ');
    for (std::size_t i = 0; i < m_aRedlineTable.size(); ++i)
    {
        const OUString* pAuthor = GetRedline(i);
        Strm().WriteChar('{');
        if (pAuthor)
            Strm().WriteCharPtr(
                msfilter::rtfutil::OutString(*pAuthor, m_eDefaultEncoding).getStr());
        Strm().WriteCharPtr(";}");
    }
    Strm().WriteChar('}').WriteCharPtr(SAL_NEWLINE_STRING);
}

void RtfExport::WriteHeadersFooters(sal_uInt8 nHeadFootFlags, const SwFrameFormat& rFormat,
                                    const SwFrameFormat& rLeftHeaderFormat,
                                    const SwFrameFormat& rLeftFooterFormat,
                                    const SwFrameFormat& rFirstPageFormat, sal_uInt8 /*nBreakCode*/,
                                    bool /*bEvenAndOddHeaders*/)
{
    // headers
    if (nHeadFootFlags & nsHdFtFlags::WW8_HEADER_EVEN)
        WriteHeaderFooter(rLeftHeaderFormat, true, OOO_STRING_SVTOOLS_RTF_HEADERL);

    if (nHeadFootFlags & nsHdFtFlags::WW8_HEADER_ODD)
        WriteHeaderFooter(rFormat, true, OOO_STRING_SVTOOLS_RTF_HEADER);

    if (nHeadFootFlags & nsHdFtFlags::WW8_HEADER_FIRST)
        WriteHeaderFooter(rFirstPageFormat, true, OOO_STRING_SVTOOLS_RTF_HEADERF, true);

    // footers
    if (nHeadFootFlags & nsHdFtFlags::WW8_FOOTER_EVEN)
        WriteHeaderFooter(rLeftFooterFormat, false, OOO_STRING_SVTOOLS_RTF_FOOTERL);

    if (nHeadFootFlags & nsHdFtFlags::WW8_FOOTER_ODD)
        WriteHeaderFooter(rFormat, false, OOO_STRING_SVTOOLS_RTF_FOOTER);

    if (nHeadFootFlags & nsHdFtFlags::WW8_FOOTER_FIRST)
        WriteHeaderFooter(rFirstPageFormat, false, OOO_STRING_SVTOOLS_RTF_FOOTERF, true);
}

void RtfExport::OutputField(const SwField* pField, ww::eField eFieldType, const OUString& rFieldCmd,
                            FieldFlags nMode)
{
    m_pAttrOutput->WriteField_Impl(pField, eFieldType, rFieldCmd, nMode);
}

void RtfExport::WriteFormData(const ::sw::mark::IFieldmark& /*rFieldmark*/)
{
    SAL_INFO("sw.rtf", "TODO: " << __func__);
}

void RtfExport::WriteHyperlinkData(const ::sw::mark::IFieldmark& /*rFieldmark*/)
{
    SAL_INFO("sw.rtf", "TODO: " << __func__);
}

void RtfExport::DoComboBox(const OUString& /*rName*/, const OUString& /*rHelp*/,
                           const OUString& /*rToolTip*/, const OUString& /*rSelected*/,
                           const uno::Sequence<OUString>& /*rListItems*/)
{
    // this is handled in RtfAttributeOutput::OutputFlyFrame_Impl
}

void RtfExport::DoFormText(const SwInputField* pField)
{
    OUString sResult = pField->ExpandField(true, nullptr);
    const OUString& rHelp = pField->GetHelp();
    OUString sName = pField->GetPar2();
    const OUString& rStatus = pField->GetToolTip();
    m_pAttrOutput->RunText().append("{" OOO_STRING_SVTOOLS_RTF_FIELD
                                    "{" OOO_STRING_SVTOOLS_RTF_IGNORE OOO_STRING_SVTOOLS_RTF_FLDINST
                                    "{ FORMTEXT }");
    m_pAttrOutput->RunText().append(
        "{" OOO_STRING_SVTOOLS_RTF_IGNORE OOO_STRING_SVTOOLS_RTF_FORMFIELD
        " {" OOO_STRING_SVTOOLS_RTF_FFTYPE "0");
    if (!rHelp.isEmpty())
        m_pAttrOutput->RunText().append(OOO_STRING_SVTOOLS_RTF_FFOWNHELP);
    if (!rStatus.isEmpty())
        m_pAttrOutput->RunText().append(OOO_STRING_SVTOOLS_RTF_FFOWNSTAT);
    m_pAttrOutput->RunText().append(OOO_STRING_SVTOOLS_RTF_FFTYPETXT "0");

    if (!sName.isEmpty())
        m_pAttrOutput->RunText().append(
            "{" OOO_STRING_SVTOOLS_RTF_IGNORE OOO_STRING_SVTOOLS_RTF_FFNAME " "
            + msfilter::rtfutil::OutString(sName, m_eDefaultEncoding) + "}");
    if (!rHelp.isEmpty())
        m_pAttrOutput->RunText().append(
            "{" OOO_STRING_SVTOOLS_RTF_IGNORE OOO_STRING_SVTOOLS_RTF_FFHELPTEXT " "
            + msfilter::rtfutil::OutString(rHelp, m_eDefaultEncoding) + "}");
    m_pAttrOutput->RunText().append(
        "{" OOO_STRING_SVTOOLS_RTF_IGNORE OOO_STRING_SVTOOLS_RTF_FFDEFTEXT " "
        + msfilter::rtfutil::OutString(sResult, m_eDefaultEncoding) + "}");
    if (!rStatus.isEmpty())
        m_pAttrOutput->RunText().append(
            "{" OOO_STRING_SVTOOLS_RTF_IGNORE OOO_STRING_SVTOOLS_RTF_FFSTATTEXT " "
            + msfilter::rtfutil::OutString(rStatus, m_eDefaultEncoding) + "}");
    m_pAttrOutput->RunText().append("}}}{" OOO_STRING_SVTOOLS_RTF_FLDRSLT " ");
    m_pAttrOutput->RunText().append(msfilter::rtfutil::OutString(sResult, m_eDefaultEncoding)
                                    + "}}");
}

sal_uInt64 RtfExport::ReplaceCr(sal_uInt8 /*nChar*/)
{
    // Completely unused for Rtf export... only here for code sharing
    // purpose with binary export

    return 0;
}

void RtfExport::WriteFonts()
{
    Strm()
        .WriteCharPtr(SAL_NEWLINE_STRING)
        .WriteChar('{')
        .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_FONTTBL);
    m_aFontHelper.WriteFontTable(*m_pAttrOutput);
    Strm().WriteChar('}');
}

void RtfExport::WriteStyles()
{
    SAL_INFO("sw.rtf", __func__ << " start");
    m_pStyles->OutputStylesTable();
    SAL_INFO("sw.rtf", __func__ << " end");
}

void RtfExport::WriteFootnoteSettings()
{
    const SwPageFootnoteInfo& rFootnoteInfo = m_rDoc.GetPageDesc(0).GetFootnoteInfo();
    // Request a separator only in case the width is larger than zero.
    bool bSeparator = double(rFootnoteInfo.GetWidth()) > 0;

    Strm()
        .WriteChar('{')
        .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_IGNORE)
        .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_FTNSEP);
    if (bSeparator)
        Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_CHFTNSEP);
    Strm().WriteChar('}');
}

void RtfExport::WriteMainText()
{
    SAL_INFO("sw.rtf", __func__ << " start");

    if (std::unique_ptr<SvxBrushItem> oBrush = getBackground(); oBrush)
    {
        Strm().WriteCharPtr(LO_STRING_SVTOOLS_RTF_VIEWBKSP).WriteChar('1');
        Strm().WriteCharPtr("{" OOO_STRING_SVTOOLS_RTF_IGNORE OOO_STRING_SVTOOLS_RTF_BACKGROUND);
        Strm().WriteCharPtr("{" OOO_STRING_SVTOOLS_RTF_SHP);
        Strm().WriteCharPtr("{" OOO_STRING_SVTOOLS_RTF_IGNORE OOO_STRING_SVTOOLS_RTF_SHPINST);

        std::vector<std::pair<OString, OString>> aProperties{
            { "shapeType", "1" },
            { "fillColor", OString::number(wwUtility::RGBToBGR(oBrush->GetColor())) }
        };
        for (const std::pair<OString, OString>& rPair : aProperties)
        {
            Strm().WriteCharPtr("{" OOO_STRING_SVTOOLS_RTF_SP "{");
            Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_SN " ");
            Strm().WriteOString(rPair.first);
            Strm().WriteCharPtr("}{" OOO_STRING_SVTOOLS_RTF_SV " ");
            Strm().WriteOString(rPair.second);
            Strm().WriteCharPtr("}}");
        }
        Strm().WriteChar('}'); // shpinst
        Strm().WriteChar('}'); // shp
        Strm().WriteChar('}'); // background
    }

    SwTableNode* pTableNode = m_pCurPam->GetNode().FindTableNode();
    if (m_pWriter && m_pWriter->m_bWriteOnlyFirstTable && pTableNode != nullptr)
    {
        m_pCurPam->GetPoint()->nNode = *pTableNode;
        m_pCurPam->GetMark()->nNode = *(pTableNode->EndOfSectionNode());
    }
    else
    {
        m_pCurPam->GetPoint()->nNode
            = m_rDoc.GetNodes().GetEndOfContent().StartOfSectionNode()->GetIndex();
    }

    WriteText();

    SAL_INFO("sw.rtf", __func__ << " end");
}

void RtfExport::WriteInfo()
{
    OString aGenerator
        = OUStringToOString(utl::DocInfoHelper::GetGeneratorString(), RTL_TEXTENCODING_UTF8);
    Strm()
        .WriteCharPtr("{" OOO_STRING_SVTOOLS_RTF_IGNORE LO_STRING_SVTOOLS_RTF_GENERATOR " ")
        .WriteOString(aGenerator)
        .WriteChar('}');
    Strm().WriteChar('{').WriteCharPtr(OOO_STRING_SVTOOLS_RTF_INFO);

    SwDocShell* pDocShell(m_rDoc.GetDocShell());
    uno::Reference<document::XDocumentProperties> xDocProps;
    if (pDocShell)
    {
        uno::Reference<document::XDocumentPropertiesSupplier> xDPS(pDocShell->GetModel(),
                                                                   uno::UNO_QUERY);
        xDocProps.set(xDPS->getDocumentProperties());
    }

    if (xDocProps.is())
    {
        // Handle user-defined properties.
        uno::Reference<beans::XPropertyContainer> xUserDefinedProperties
            = xDocProps->getUserDefinedProperties();
        if (xUserDefinedProperties.is())
        {
            uno::Reference<beans::XPropertySet> xPropertySet(xUserDefinedProperties,
                                                             uno::UNO_QUERY);
            uno::Reference<beans::XPropertySetInfo> xPropertySetInfo
                = xPropertySet->getPropertySetInfo();
            // Do we have explicit markup in RTF for this property name?
            if (xPropertySetInfo->hasPropertyByName("Company"))
            {
                OUString aValue;
                xPropertySet->getPropertyValue("Company") >>= aValue;
                OutUnicode(OOO_STRING_SVTOOLS_RTF_IGNORE OOO_STRING_SVTOOLS_RTF_COMPANY, aValue);
            }
        }

        OutUnicode(OOO_STRING_SVTOOLS_RTF_TITLE, xDocProps->getTitle(), true);
        OutUnicode(OOO_STRING_SVTOOLS_RTF_SUBJECT, xDocProps->getSubject());

        OutUnicode(OOO_STRING_SVTOOLS_RTF_KEYWORDS,
                   ::comphelper::string::convertCommaSeparated(xDocProps->getKeywords()));
        OutUnicode(OOO_STRING_SVTOOLS_RTF_DOCCOMM, xDocProps->getDescription());

        OutUnicode(OOO_STRING_SVTOOLS_RTF_AUTHOR, xDocProps->getAuthor());
        OutDateTime(OOO_STRING_SVTOOLS_RTF_CREATIM, xDocProps->getCreationDate());

        OutUnicode(OOO_STRING_SVTOOLS_RTF_AUTHOR, xDocProps->getModifiedBy());
        OutDateTime(OOO_STRING_SVTOOLS_RTF_REVTIM, xDocProps->getModificationDate());

        OutDateTime(OOO_STRING_SVTOOLS_RTF_PRINTIM, xDocProps->getPrintDate());
    }

    Strm().WriteChar('}');
}

void RtfExport::WriteUserPropType(int nType)
{
    Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_PROPTYPE);
    OutULong(nType);
}

void RtfExport::WriteUserPropValue(const OUString& rValue)
{
    Strm().WriteCharPtr("{" OOO_STRING_SVTOOLS_RTF_STATICVAL " ");
    Strm().WriteOString(msfilter::rtfutil::OutString(rValue, m_eDefaultEncoding));
    Strm().WriteChar('}');
}

void RtfExport::WriteUserProps()
{
    Strm().WriteChar('{').WriteCharPtr(
        OOO_STRING_SVTOOLS_RTF_IGNORE OOO_STRING_SVTOOLS_RTF_USERPROPS);

    SwDocShell* pDocShell(m_rDoc.GetDocShell());
    uno::Reference<document::XDocumentProperties> xDocProps;
    if (pDocShell)
    {
        uno::Reference<document::XDocumentPropertiesSupplier> xDPS(pDocShell->GetModel(),
                                                                   uno::UNO_QUERY);
        xDocProps.set(xDPS->getDocumentProperties());
    }
    else
    {
        // Clipboard document, read metadata from the meta field manager.
        sw::MetaFieldManager& rManager = m_rDoc.GetMetaFieldManager();
        xDocProps.set(rManager.getDocumentProperties());
    }

    if (xDocProps.is())
    {
        // Handle user-defined properties.
        uno::Reference<beans::XPropertyContainer> xUserDefinedProperties
            = xDocProps->getUserDefinedProperties();
        if (xUserDefinedProperties.is())
        {
            uno::Reference<beans::XPropertySet> xPropertySet(xUserDefinedProperties,
                                                             uno::UNO_QUERY);
            const uno::Sequence<beans::Property> aProperties
                = xPropertySet->getPropertySetInfo()->getProperties();

            for (const beans::Property& rProperty : aProperties)
            {
                if (rProperty.Name.startsWith("Company"))
                    // We have explicit markup in RTF for this property.
                    continue;

                // Property name.
                Strm().WriteCharPtr("{" OOO_STRING_SVTOOLS_RTF_PROPNAME " ");
                Strm().WriteCharPtr(
                    msfilter::rtfutil::OutString(rProperty.Name, m_eDefaultEncoding).getStr());
                Strm().WriteChar('}');

                // Property value.
                OUString aValue;
                double fValue;
                bool bValue;
                util::DateTime aDate;
                uno::Any aAny = xPropertySet->getPropertyValue(rProperty.Name);
                if (aAny >>= bValue)
                {
                    WriteUserPropType(11);
                    WriteUserPropValue(OUString::number(static_cast<int>(bValue)));
                }
                else if (aAny >>= aValue)
                {
                    WriteUserPropType(30);
                    WriteUserPropValue(aValue);
                }
                else if (aAny >>= fValue)
                {
                    aValue = OUString::number(fValue);
                    if (aValue.indexOf('.') == -1)
                    {
                        // Integer.
                        WriteUserPropType(3);
                        WriteUserPropValue(aValue);
                    }
                    else
                    {
                        // Real number.
                        WriteUserPropType(5);
                        WriteUserPropValue(aValue);
                    }
                }
                else if (aAny >>= aDate)
                {
                    WriteUserPropType(64);
                    // Format is 'YYYY. MM. DD.'.
                    aValue += OUString::number(aDate.Year) + ". ";
                    if (aDate.Month < 10)
                        aValue += "0";
                    aValue += OUString::number(aDate.Month) + ". ";
                    if (aDate.Day < 10)
                        aValue += "0";
                    aValue += OUString::number(aDate.Day) + ".";
                    WriteUserPropValue(aValue);
                }
            }
        }
    }

    Strm().WriteChar('}');
}

void RtfExport::WritePageDescTable()
{
    // Write page descriptions (page styles)
    std::size_t nSize = m_rDoc.GetPageDescCnt();
    if (!nSize)
        return;

    Strm().WriteCharPtr(SAL_NEWLINE_STRING);
    m_bOutPageDescs = true;
    Strm()
        .WriteChar('{')
        .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_IGNORE)
        .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_PGDSCTBL);
    for (std::size_t n = 0; n < nSize; ++n)
    {
        const SwPageDesc& rPageDesc = m_rDoc.GetPageDesc(n);

        Strm()
            .WriteCharPtr(SAL_NEWLINE_STRING)
            .WriteChar('{')
            .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_PGDSC);
        OutULong(n).WriteCharPtr(OOO_STRING_SVTOOLS_RTF_PGDSCUSE);
        OutULong(static_cast<sal_uLong>(rPageDesc.ReadUseOn()));

        OutPageDescription(rPageDesc, false);

        // search for the next page description
        std::size_t i = nSize;
        while (i)
            if (rPageDesc.GetFollow() == &m_rDoc.GetPageDesc(--i))
                break;
        Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_PGDSCNXT);
        OutULong(i).WriteChar(' ');
        Strm()
            .WriteCharPtr(
                msfilter::rtfutil::OutString(rPageDesc.GetName(), m_eDefaultEncoding).getStr())
            .WriteCharPtr(";}");
    }
    Strm().WriteChar('}').WriteCharPtr(SAL_NEWLINE_STRING);
    m_bOutPageDescs = false;

    // reset table infos, otherwise the depth of the cells will be incorrect,
    // in case the page style (header or footer) had tables
    m_pTableInfo = std::make_shared<ww8::WW8TableInfo>();
}

ErrCode RtfExport::ExportDocument_Impl()
{
    // Make the header
    Strm()
        .WriteChar('{')
        .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_RTF)
        .WriteChar('1')
        .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_ANSI);
    Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_DEFF);
    OutULong(m_aFontHelper.GetId(m_rDoc.GetAttrPool().GetDefaultItem(RES_CHRATR_FONT)));
    // If this not exist, MS don't understand our ansi characters (0x80-0xff).
    Strm().WriteCharPtr("\\adeflang1025");

    // Font table
    WriteFonts();

    m_pStyles = std::make_unique<MSWordStyles>(*this);
    // Color and stylesheet table
    WriteStyles();

    // List table
    BuildNumbering();
    WriteNumbering();

    WriteRevTab();

    WriteInfo();
    WriteUserProps();
    // Default TabSize
    Strm()
        .WriteOString(m_pAttrOutput->GetTabStop().makeStringAndClear())
        .WriteCharPtr(SAL_NEWLINE_STRING);

    // Automatic hyphenation: it's a global setting in Word, it's a paragraph setting in Writer.
    // Set it's value to "auto" and disable on paragraph level, if no hyphenation is used there.
    Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_HYPHAUTO);
    OutULong(1);

    // Zoom
    SwViewShell* pViewShell(m_rDoc.getIDocumentLayoutAccess().GetCurrentViewShell());
    if (pViewShell && pViewShell->GetViewOptions()->GetZoomType() == SvxZoomType::PERCENT)
    {
        Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_VIEWSCALE);
        OutULong(pViewShell->GetViewOptions()->GetZoom());
    }
    // Record changes?
    if (RedlineFlags::On & m_nOrigRedlineFlags)
        Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_REVISIONS);
    // Mirror margins?
    if ((UseOnPage::Mirror & m_rDoc.GetPageDesc(0).ReadUseOn()) == UseOnPage::Mirror)
        Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_MARGMIRROR);

    // Gutter at top?
    if (m_rDoc.getIDocumentSettingAccess().get(DocumentSettingId::GUTTER_AT_TOP))
    {
        Strm().WriteCharPtr(LO_STRING_SVTOOLS_RTF_GUTTERPRL);
    }

    // Init sections
    m_pSections = std::make_unique<MSWordSections>(*this);

    // Page description
    WritePageDescTable();

    // Enable form protection by default if needed, as there is no switch to
    // enable it on a per-section basis. OTOH don't always enable it as it
    // breaks moving of drawings - so write it only in case there is really a
    // protected section in the document.
    for (auto const& pSectionFormat : m_rDoc.GetSections())
    {
        if (!pSectionFormat->IsInUndo() && pSectionFormat->GetProtect().IsContentProtected())
        {
            Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_FORMPROT);
            break;
        }
    }

    // enable form field shading
    Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_FORMSHADE);

    // size and empty margins of the page
    if (m_rDoc.GetPageDescCnt())
    {
        // Seeking the first SwFormatPageDesc. If no set, the default is valid
        const SwFormatPageDesc* pSttPgDsc = nullptr;
        {
            const SwNode& rSttNd
                = *m_rDoc.GetNodes()[m_rDoc.GetNodes().GetEndOfExtras().GetIndex() + 2];
            const SfxItemSet* pSet = nullptr;

            if (rSttNd.IsContentNode())
                pSet = &rSttNd.GetContentNode()->GetSwAttrSet();
            else if (rSttNd.IsTableNode())
                pSet = &rSttNd.GetTableNode()->GetTable().GetFrameFormat()->GetAttrSet();

            else if (rSttNd.IsSectionNode())
                pSet = &rSttNd.GetSectionNode()->GetSection().GetFormat()->GetAttrSet();

            if (pSet)
            {
                std::size_t nPosInDoc;
                pSttPgDsc = &pSet->Get(RES_PAGEDESC);
                if (!pSttPgDsc->GetPageDesc())
                    pSttPgDsc = nullptr;
                else if (m_rDoc.FindPageDesc(pSttPgDsc->GetPageDesc()->GetName(), &nPosInDoc))
                {
                    Strm()
                        .WriteChar('{')
                        .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_IGNORE)
                        .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_PGDSCNO);
                    OutULong(nPosInDoc).WriteChar('}');
                }
            }
        }
        const SwPageDesc& rPageDesc = pSttPgDsc ? *pSttPgDsc->GetPageDesc() : m_rDoc.GetPageDesc(0);
        const SwFrameFormat& rFormatPage = rPageDesc.GetMaster();

        {
            if (rPageDesc.GetLandscape())
                Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_LANDSCAPE);

            const SwFormatFrameSize& rSz = rFormatPage.GetFrameSize();
            // Clipboard document is always created without a printer, then
            // the size will be always LONG_MAX! Solution then is to use A4
            if (LONG_MAX == rSz.GetHeight() || LONG_MAX == rSz.GetWidth())
            {
                Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_PAPERH);
                Size a4 = SvxPaperInfo::GetPaperSize(PAPER_A4);
                OutULong(a4.Height()).WriteCharPtr(OOO_STRING_SVTOOLS_RTF_PAPERW);
                OutULong(a4.Width());
            }
            else
            {
                Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_PAPERH);
                OutULong(rSz.GetHeight()).WriteCharPtr(OOO_STRING_SVTOOLS_RTF_PAPERW);
                OutULong(rSz.GetWidth());
            }
        }

        {
            const SvxLRSpaceItem& rLR = rFormatPage.GetLRSpace();
            Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_MARGL);
            OutLong(rLR.GetLeft()).WriteCharPtr(OOO_STRING_SVTOOLS_RTF_MARGR);
            OutLong(rLR.GetRight());
        }

        {
            const SvxULSpaceItem& rUL = rFormatPage.GetULSpace();
            Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_MARGT);
            OutLong(rUL.GetUpper()).WriteCharPtr(OOO_STRING_SVTOOLS_RTF_MARGB);
            OutLong(rUL.GetLower());
        }

        Strm()
            .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_SECTD)
            .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_SBKNONE);
        m_pAttrOutput->SectFootnoteEndnotePr();
        // All sections are unlocked by default
        Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_SECTUNLOCKED);
        OutLong(1);
        OutPageDescription(rPageDesc, true); // Changed bCheckForFirstPage to true so headers
        // following title page are correctly added - i13107
        if (pSttPgDsc)
        {
            m_pCurrentPageDesc = &rPageDesc;
        }
    }

    // line numbering
    const SwLineNumberInfo& rLnNumInfo = m_rDoc.GetLineNumberInfo();
    if (rLnNumInfo.IsPaintLineNumbers())
    {
        sal_uLong nLnNumRestartNo = 0;
        if (const WW8_SepInfo* pSectionInfo = m_pSections->CurrentSectionInfo())
            nLnNumRestartNo = pSectionInfo->nLnNumRestartNo;

        AttrOutput().SectionLineNumbering(nLnNumRestartNo, rLnNumInfo);
    }

    {
        // write the footnotes and endnotes-out Info
        const SwFootnoteInfo& rFootnoteInfo = m_rDoc.GetFootnoteInfo();

        const char* pOut = FTNPOS_CHAPTER == rFootnoteInfo.m_ePos ? OOO_STRING_SVTOOLS_RTF_ENDDOC
                                                                  : OOO_STRING_SVTOOLS_RTF_FTNBJ;
        Strm().WriteCharPtr(pOut).WriteCharPtr(OOO_STRING_SVTOOLS_RTF_FTNSTART);
        OutLong(rFootnoteInfo.m_nFootnoteOffset + 1);

        switch (rFootnoteInfo.m_eNum)
        {
            case FTNNUM_PAGE:
                pOut = OOO_STRING_SVTOOLS_RTF_FTNRSTPG;
                break;
            case FTNNUM_DOC:
                pOut = OOO_STRING_SVTOOLS_RTF_FTNRSTCONT;
                break;
            default:
                pOut = OOO_STRING_SVTOOLS_RTF_FTNRESTART;
                break;
        }
        Strm().WriteCharPtr(pOut);

        switch (rFootnoteInfo.m_aFormat.GetNumberingType())
        {
            case SVX_NUM_CHARS_LOWER_LETTER:
            case SVX_NUM_CHARS_LOWER_LETTER_N:
                pOut = OOO_STRING_SVTOOLS_RTF_FTNNALC;
                break;
            case SVX_NUM_CHARS_UPPER_LETTER:
            case SVX_NUM_CHARS_UPPER_LETTER_N:
                pOut = OOO_STRING_SVTOOLS_RTF_FTNNAUC;
                break;
            case SVX_NUM_ROMAN_LOWER:
                pOut = OOO_STRING_SVTOOLS_RTF_FTNNRLC;
                break;
            case SVX_NUM_ROMAN_UPPER:
                pOut = OOO_STRING_SVTOOLS_RTF_FTNNRUC;
                break;
            case SVX_NUM_SYMBOL_CHICAGO:
                pOut = OOO_STRING_SVTOOLS_RTF_FTNNCHI;
                break;
            default:
                pOut = OOO_STRING_SVTOOLS_RTF_FTNNAR;
                break;
        }
        Strm().WriteCharPtr(pOut);

        const SwEndNoteInfo& rEndNoteInfo = m_rDoc.GetEndNoteInfo();

        Strm()
            .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_AENDDOC)
            .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_AFTNRSTCONT)
            .WriteCharPtr(OOO_STRING_SVTOOLS_RTF_AFTNSTART);
        OutLong(rEndNoteInfo.m_nFootnoteOffset + 1);

        switch (rEndNoteInfo.m_aFormat.GetNumberingType())
        {
            case SVX_NUM_CHARS_LOWER_LETTER:
            case SVX_NUM_CHARS_LOWER_LETTER_N:
                pOut = OOO_STRING_SVTOOLS_RTF_AFTNNALC;
                break;
            case SVX_NUM_CHARS_UPPER_LETTER:
            case SVX_NUM_CHARS_UPPER_LETTER_N:
                pOut = OOO_STRING_SVTOOLS_RTF_AFTNNAUC;
                break;
            case SVX_NUM_ROMAN_LOWER:
                pOut = OOO_STRING_SVTOOLS_RTF_AFTNNRLC;
                break;
            case SVX_NUM_ROMAN_UPPER:
                pOut = OOO_STRING_SVTOOLS_RTF_AFTNNRUC;
                break;
            case SVX_NUM_SYMBOL_CHICAGO:
                pOut = OOO_STRING_SVTOOLS_RTF_AFTNNCHI;
                break;
            default:
                pOut = OOO_STRING_SVTOOLS_RTF_AFTNNAR;
                break;
        }
        Strm().WriteCharPtr(pOut);
    }

    if (!m_rDoc.getIDocumentSettingAccess().get(DocumentSettingId::PARA_SPACE_MAX))
        // RTF default is true, so write compat flag if this should be false.
        Strm().WriteCharPtr(LO_STRING_SVTOOLS_RTF_HTMAUTSP);

    Strm().WriteCharPtr(SAL_NEWLINE_STRING);

    WriteFootnoteSettings();

    WriteMainText();

    Strm().WriteChar('}');

    return ERRCODE_NONE;
}

void RtfExport::PrepareNewPageDesc(const SfxItemSet* pSet, const SwNode& rNd,
                                   const SwFormatPageDesc* pNewPgDescFormat,
                                   const SwPageDesc* pNewPgDesc, bool bExtraPageBreak)
{
    const SwSectionFormat* pFormat = GetSectionFormat(rNd);
    const sal_uLong nLnNm = GetSectionLineNo(pSet, rNd);

    OSL_ENSURE(pNewPgDescFormat || pNewPgDesc, "Neither page desc format nor page desc provided.");

    if (pNewPgDescFormat)
        m_pSections->AppendSection(*pNewPgDescFormat, rNd, pFormat, nLnNm);
    else if (pNewPgDesc)
        m_pSections->AppendSection(pNewPgDesc, rNd, pFormat, nLnNm);

    // Don't insert a page break, when we're changing page style just because the next page has to be a different one.
    if (!m_pAttrOutput->GetPrevPageDesc()
        || m_pAttrOutput->GetPrevPageDesc()->GetFollow() != pNewPgDesc)
        AttrOutput().SectionBreak(msword::PageBreak, false, m_pSections->CurrentSectionInfo(),
                                  bExtraPageBreak);
}

bool RtfExport::DisallowInheritingOutlineNumbering(const SwFormat& rFormat)
{
    bool bRet(false);

    if (SfxItemState::SET != rFormat.GetItemState(RES_PARATR_NUMRULE, false))
    {
        if (const SwFormat* pParent = rFormat.DerivedFrom())
        {
            if (static_cast<const SwTextFormatColl*>(pParent)
                    ->IsAssignedToListLevelOfOutlineStyle())
            {
                // Level 9 disables the outline
                Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_LEVEL).WriteInt32(9);

                bRet = true;
            }
        }
    }

    return bRet;
}

void RtfExport::OutputEndNode(const SwEndNode& rEndNode)
{
    if (TXT_MAINTEXT == m_nTextTyp && rEndNode.StartOfSectionNode()->IsTableNode())
        // End node of a table: see if a section break should be written after the table.
        AttrOutput().SectionBreaks(rEndNode);
}

void RtfExport::OutputGrfNode(const SwGrfNode& /*rGrfNode*/)
{
    /* noop, see RtfAttributeOutput::FlyFrameGraphic */
}

void RtfExport::OutputOLENode(const SwOLENode& /*rOLENode*/)
{
    /* noop, see RtfAttributeOutput::FlyFrameOLE */
}

void RtfExport::OutputLinkedOLE(const OUString& /*rLinked*/) {}

void RtfExport::OutputTextNode(SwTextNode& rNode)
{
    m_nCurrentNodeIndex = rNode.GetIndex();
    if (!m_bOutOutlineOnly || rNode.IsOutline())
        MSWordExportBase::OutputTextNode(rNode);
    m_nCurrentNodeIndex = SwNodeOffset(0);
}

void RtfExport::AppendSection(const SwPageDesc* pPageDesc, const SwSectionFormat* pFormat,
                              sal_uLong nLnNum)
{
    m_pSections->AppendSection(pPageDesc, pFormat, nLnNum);
    AttrOutput().SectionBreak(msword::PageBreak, false, m_pSections->CurrentSectionInfo());
}

RtfExport::RtfExport(RtfExportFilter* pFilter, SwDoc& rDocument,
                     std::shared_ptr<SwUnoCursor>& pCurrentPam, SwPaM& rOriginalPam,
                     Writer* pWriter, bool bOutOutlineOnly)
    : MSWordExportBase(rDocument, pCurrentPam, &rOriginalPam)
    , m_pFilter(pFilter)
    , m_pWriter(pWriter)
    , m_bOutOutlineOnly(bOutOutlineOnly)
    , m_eDefaultEncoding(
          rtl_getTextEncodingFromWindowsCharset(sw::ms::rtl_TextEncodingToWinCharset(DEF_ENCODING)))
    , m_eCurrentEncoding(m_eDefaultEncoding)
    , m_bRTFFlySyntax(false)
    , m_nCurrentNodeIndex(0)
{
    m_bExportModeRTF = true;
    // the attribute output for the document
    m_pAttrOutput = std::make_unique<RtfAttributeOutput>(*this);
    // that just causes problems for RTF
    m_bSubstituteBullets = false;
    // needed to have a complete font table
    m_aFontHelper.bLoadAllFonts = true;
    // the related SdrExport
    m_pSdrExport = std::make_unique<RtfSdrExport>(*this);

    if (!m_pWriter)
        m_pWriter = &m_pFilter->GetWriter();
}

RtfExport::~RtfExport() = default;

SvStream& RtfExport::Strm()
{
    if (m_pStream)
        return *m_pStream;

    return m_pWriter->Strm();
}

void RtfExport::setStream() { m_pStream = std::make_unique<SvMemoryStream>(); }

OString RtfExport::getStream()
{
    OString aRet;

    if (m_pStream)
        aRet = OString(static_cast<const char*>(m_pStream->GetData()), m_pStream->Tell());

    return aRet;
}

void RtfExport::resetStream() { m_pStream.reset(); }

SvStream& RtfExport::OutULong(sal_uLong nVal) { return Writer::OutULong(Strm(), nVal); }

SvStream& RtfExport::OutLong(tools::Long nVal) { return Writer::OutLong(Strm(), nVal); }

void RtfExport::OutUnicode(const char* pToken, const OUString& rContent, bool bUpr)
{
    if (rContent.isEmpty())
        return;

    if (!bUpr)
    {
        Strm().WriteChar('{').WriteCharPtr(pToken).WriteChar(' ');
        Strm().WriteCharPtr(msfilter::rtfutil::OutString(rContent, m_eCurrentEncoding).getStr());
        Strm().WriteChar('}');
    }
    else
        Strm().WriteCharPtr(
            msfilter::rtfutil::OutStringUpr(pToken, rContent, m_eCurrentEncoding).getStr());
}

void RtfExport::OutDateTime(const char* pStr, const util::DateTime& rDT)
{
    Strm().WriteChar('{').WriteCharPtr(pStr).WriteCharPtr(OOO_STRING_SVTOOLS_RTF_YR);
    OutULong(rDT.Year).WriteCharPtr(OOO_STRING_SVTOOLS_RTF_MO);
    OutULong(rDT.Month).WriteCharPtr(OOO_STRING_SVTOOLS_RTF_DY);
    OutULong(rDT.Day).WriteCharPtr(OOO_STRING_SVTOOLS_RTF_HR);
    OutULong(rDT.Hours).WriteCharPtr(OOO_STRING_SVTOOLS_RTF_MIN);
    OutULong(rDT.Minutes).WriteChar('}');
}

sal_uInt16 RtfExport::GetColor(const Color& rColor) const
{
    for (const auto& rEntry : m_aColTable)
        if (rEntry.second == rColor)
        {
            SAL_INFO("sw.rtf", __func__ << " returning " << rEntry.first << " (" << rColor.GetRed()
                                        << "," << rColor.GetGreen() << "," << rColor.GetBlue()
                                        << ")");
            return rEntry.first;
        }
    OSL_FAIL("No such Color in m_aColTable!");
    return 0;
}

void RtfExport::InsColor(const Color& rCol)
{
    sal_uInt16 n;
    bool bAutoColorInTable = false;
    for (const auto& rEntry : m_aColTable)
    {
        if (rEntry.second == rCol)
            return; // Already in the table
        if (rEntry.second == COL_AUTO)
            bAutoColorInTable = true;
    }
    if (rCol == COL_AUTO)
        // COL_AUTO gets value 0
        n = 0;
    else
    {
        // other colors get values >0
        n = m_aColTable.size();
        if (!bAutoColorInTable)
            // reserve value "0" for COL_AUTO (if COL_AUTO wasn't inserted until now)
            n++;
    }
    m_aColTable.insert(std::pair<sal_uInt16, Color>(n, rCol));
}

void RtfExport::InsColorLine(const SvxBoxItem& rBox)
{
    const editeng::SvxBorderLine* pLine = nullptr;

    if (rBox.GetTop())
    {
        pLine = rBox.GetTop();
        InsColor(pLine->GetColor());
    }
    if (rBox.GetBottom() && pLine != rBox.GetBottom())
    {
        pLine = rBox.GetBottom();
        InsColor(pLine->GetColor());
    }
    if (rBox.GetLeft() && pLine != rBox.GetLeft())
    {
        pLine = rBox.GetLeft();
        InsColor(pLine->GetColor());
    }
    if (rBox.GetRight() && pLine != rBox.GetRight())
        InsColor(rBox.GetRight()->GetColor());
}

void RtfExport::OutColorTable()
{
    // Build the table from rPool since the colors provided to
    // RtfAttributeOutput callbacks are too late.
    const SfxItemPool& rPool = m_rDoc.GetAttrPool();

    // MSO Word uses a default color table with 16 colors (which is used e.g. for highlighting)
    InsColor(COL_BLACK);
    InsColor(COL_LIGHTBLUE);
    InsColor(COL_LIGHTCYAN);
    InsColor(COL_LIGHTGREEN);
    InsColor(COL_LIGHTMAGENTA);
    InsColor(COL_LIGHTRED);
    InsColor(COL_YELLOW);
    InsColor(COL_WHITE);
    InsColor(COL_BLUE);
    InsColor(COL_CYAN);
    InsColor(COL_GREEN);
    InsColor(COL_MAGENTA);
    InsColor(COL_RED);
    InsColor(COL_BROWN);
    InsColor(COL_GRAY);
    InsColor(COL_LIGHTGRAY);

    // char color
    {
        auto pCol = GetDfltAttr(RES_CHRATR_COLOR);
        InsColor(pCol->GetValue());
        pCol = rPool.GetPoolDefaultItem(RES_CHRATR_COLOR);
        if (pCol)
            InsColor(pCol->GetValue());
        for (const SfxPoolItem* pItem : rPool.GetItemSurrogates(RES_CHRATR_COLOR))
        {
            pCol = dynamic_cast<const SvxColorItem*>(pItem);
            if (pCol)
                InsColor(pCol->GetValue());
        }

        auto pUnder = GetDfltAttr(RES_CHRATR_UNDERLINE);
        InsColor(pUnder->GetColor());
        for (const SfxPoolItem* pItem : rPool.GetItemSurrogates(RES_CHRATR_UNDERLINE))
        {
            pUnder = dynamic_cast<const SvxUnderlineItem*>(pItem);
            if (pUnder)
                InsColor(pUnder->GetColor());
        }

        auto pOver = GetDfltAttr(RES_CHRATR_OVERLINE);
        InsColor(pOver->GetColor());
        for (const SfxPoolItem* pItem : rPool.GetItemSurrogates(RES_CHRATR_OVERLINE))
        {
            pOver = dynamic_cast<const SvxOverlineItem*>(pItem);
            if (pOver)
                InsColor(pOver->GetColor());
        }
    }

    // background color
    static const sal_uInt16 aBrushIds[] = { RES_BACKGROUND, RES_CHRATR_BACKGROUND, 0 };

    for (const sal_uInt16* pIds = aBrushIds; *pIds; ++pIds)
    {
        auto pBackground = static_cast<const SvxBrushItem*>(GetDfltAttr(*pIds));
        InsColor(pBackground->GetColor());
        pBackground = static_cast<const SvxBrushItem*>(rPool.GetPoolDefaultItem(*pIds));
        if (pBackground)
        {
            InsColor(pBackground->GetColor());
        }
        for (const SfxPoolItem* pItem : rPool.GetItemSurrogates(*pIds))
        {
            pBackground = static_cast<const SvxBrushItem*>(pItem);
            if (pBackground)
            {
                InsColor(pBackground->GetColor());
            }
        }
    }

    // shadow color
    {
        auto pShadow = GetDfltAttr(RES_SHADOW);
        InsColor(pShadow->GetColor());
        pShadow = rPool.GetPoolDefaultItem(RES_SHADOW);
        if (nullptr != pShadow)
        {
            InsColor(pShadow->GetColor());
        }
        for (const SfxPoolItem* pItem : rPool.GetItemSurrogates(RES_SHADOW))
        {
            pShadow = dynamic_cast<const SvxShadowItem*>(pItem);
            if (pShadow)
            {
                InsColor(pShadow->GetColor());
            }
        }
    }

    // frame border color
    {
        const SvxBoxItem* pBox = rPool.GetPoolDefaultItem(RES_BOX);
        if (nullptr != pBox)
            InsColorLine(*pBox);
        for (const SfxPoolItem* pItem : rPool.GetItemSurrogates(RES_BOX))
        {
            pBox = dynamic_cast<const SvxBoxItem*>(pItem);
            if (pBox)
                InsColorLine(*pBox);
        }
    }

    {
        const SvxBoxItem* pCharBox = rPool.GetPoolDefaultItem(RES_CHRATR_BOX);
        if (pCharBox)
            InsColorLine(*pCharBox);
        for (const SfxPoolItem* pItem : rPool.GetItemSurrogates(RES_CHRATR_BOX))
        {
            pCharBox = dynamic_cast<const SvxBoxItem*>(pItem);
            if (pCharBox)
                InsColorLine(*pCharBox);
        }
    }

    // TextFrame or paragraph background solid fill.
    for (const SfxPoolItem* pItem : rPool.GetItemSurrogates(XATTR_FILLCOLOR))
    {
        if (auto pColorItem = dynamic_cast<const XFillColorItem*>(pItem))
            InsColor(pColorItem->GetColorValue());
    }

    for (std::size_t n = 0; n < m_aColTable.size(); ++n)
    {
        const Color& rCol = m_aColTable[n];
        if (n || COL_AUTO != rCol)
        {
            Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_RED);
            OutULong(rCol.GetRed()).WriteCharPtr(OOO_STRING_SVTOOLS_RTF_GREEN);
            OutULong(rCol.GetGreen()).WriteCharPtr(OOO_STRING_SVTOOLS_RTF_BLUE);
            OutULong(rCol.GetBlue());
        }
        Strm().WriteChar(';');
    }
}

void RtfExport::InsStyle(sal_uInt16 nId, const OString& rStyle)
{
    m_aStyTable.insert(std::pair<sal_uInt16, OString>(nId, rStyle));
}

OString* RtfExport::GetStyle(sal_uInt16 nId)
{
    auto it = m_aStyTable.find(nId);
    if (it != m_aStyTable.end())
        return &it->second;
    return nullptr;
}

sal_uInt16 RtfExport::GetRedline(const OUString& rAuthor)
{
    auto it = m_aRedlineTable.find(rAuthor);
    if (it != m_aRedlineTable.end())
        return it->second;

    const sal_uInt16 nId = m_aRedlineTable.size();
    m_aRedlineTable.insert(std::pair<OUString, sal_uInt16>(rAuthor, nId));
    return nId;
}

const OUString* RtfExport::GetRedline(sal_uInt16 nId)
{
    for (const auto& rEntry : m_aRedlineTable)
        if (rEntry.second == nId)
            return &rEntry.first;
    return nullptr;
}

void RtfExport::OutPageDescription(const SwPageDesc& rPgDsc, bool bCheckForFirstPage)
{
    SAL_INFO("sw.rtf", __func__ << " start");
    const SwPageDesc* pSave = m_pCurrentPageDesc;

    m_pCurrentPageDesc = &rPgDsc;
    if (bCheckForFirstPage && m_pCurrentPageDesc->GetFollow()
        && m_pCurrentPageDesc->GetFollow() != m_pCurrentPageDesc)
        m_pCurrentPageDesc = m_pCurrentPageDesc->GetFollow();

    if (m_pCurrentPageDesc->GetLandscape())
        Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_LNDSCPSXN);

    const SwFormat* pFormat = &m_pCurrentPageDesc->GetMaster(); //GetLeft();
    m_bOutPageDescs = true;
    if (m_pCurrentPageDesc != &rPgDsc)
        m_pFirstPageItemSet = &rPgDsc.GetMaster().GetAttrSet();
    OutputFormat(*pFormat, true, false);
    m_pFirstPageItemSet = nullptr;
    m_bOutPageDescs = false;

    // normal header / footer (without a style)
    const SfxPoolItem* pItem;
    if (m_pCurrentPageDesc->GetLeft().GetAttrSet().GetItemState(RES_HEADER, false, &pItem)
        == SfxItemState::SET)
        WriteHeaderFooter(*pItem, true);
    if (m_pCurrentPageDesc->GetLeft().GetAttrSet().GetItemState(RES_FOOTER, false, &pItem)
        == SfxItemState::SET)
        WriteHeaderFooter(*pItem, false);

    // title page
    if (m_pCurrentPageDesc != &rPgDsc)
    {
        Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_TITLEPG);
        m_pCurrentPageDesc = &rPgDsc;
        if (m_pCurrentPageDesc->GetMaster().GetAttrSet().GetItemState(RES_HEADER, false, &pItem)
            == SfxItemState::SET)
            WriteHeaderFooter(*pItem, true);
        if (m_pCurrentPageDesc->GetMaster().GetAttrSet().GetItemState(RES_FOOTER, false, &pItem)
            == SfxItemState::SET)
            WriteHeaderFooter(*pItem, false);
    }

    // numbering type
    AttrOutput().SectionPageNumbering(m_pCurrentPageDesc->GetNumType().GetNumberingType(),
                                      std::nullopt);

    m_pCurrentPageDesc = pSave;
    SAL_INFO("sw.rtf", __func__ << " end");
}

void RtfExport::WriteHeaderFooter(const SfxPoolItem& rItem, bool bHeader)
{
    if (bHeader)
    {
        const auto& rHeader = static_cast<const SwFormatHeader&>(rItem);
        if (!rHeader.IsActive())
            return;
    }
    else
    {
        const auto& rFooter = static_cast<const SwFormatFooter&>(rItem);
        if (!rFooter.IsActive())
            return;
    }

    SAL_INFO("sw.rtf", __func__ << " start");

    const char* pStr = (bHeader ? OOO_STRING_SVTOOLS_RTF_HEADER : OOO_STRING_SVTOOLS_RTF_FOOTER);
    /* is this a title page? */
    if (m_pCurrentPageDesc->GetFollow() && m_pCurrentPageDesc->GetFollow() != m_pCurrentPageDesc)
    {
        Strm().WriteCharPtr(OOO_STRING_SVTOOLS_RTF_TITLEPG);
        pStr = (bHeader ? OOO_STRING_SVTOOLS_RTF_HEADERF : OOO_STRING_SVTOOLS_RTF_FOOTERF);
    }
    Strm().WriteChar('{').WriteCharPtr(pStr);
    WriteHeaderFooterText(m_pCurrentPageDesc->GetMaster(), bHeader);
    Strm().WriteChar('}');

    SAL_INFO("sw.rtf", __func__ << " end");
}

void RtfExport::WriteHeaderFooter(const SwFrameFormat& rFormat, bool bHeader, const char* pStr,
                                  bool bTitlepg)
{
    SAL_INFO("sw.rtf", __func__ << " start");

    m_pAttrOutput->WriteHeaderFooter_Impl(rFormat, bHeader, pStr, bTitlepg);

    SAL_INFO("sw.rtf", __func__ << " end");
}

namespace
{
/// Glue class to call RtfExport as an internal filter, needed by copy&paste support.
class SwRTFWriter : public Writer
{
private:
    bool m_bOutOutlineOnly;

public:
    SwRTFWriter(std::u16string_view rFilterName, const OUString& rBaseURL);

    ErrCode WriteStream() override;
};
}

SwRTFWriter::SwRTFWriter(std::u16string_view rFilterName, const OUString& rBaseURL)
{
    SetBaseURL(rBaseURL);
    // export outline nodes, only (send outline to clipboard/presentation)
    m_bOutOutlineOnly = o3tl::starts_with(rFilterName, u"O");
}

ErrCode SwRTFWriter::WriteStream()
{
    std::shared_ptr<SwUnoCursor> pCurPam(m_pDoc->CreateUnoCursor(*m_pCurrentPam->End(), false));
    pCurPam->SetMark();
    *pCurPam->GetPoint() = *m_pCurrentPam->Start();
    RtfExport aExport(nullptr, *m_pDoc, pCurPam, *m_pCurrentPam, this, m_bOutOutlineOnly);
    aExport.ExportDocument(true);
    return ERRCODE_NONE;
}

extern "C" SAL_DLLPUBLIC_EXPORT void ExportRTF(std::u16string_view rFltName,
                                               const OUString& rBaseURL, WriterRef& xRet)
{
    xRet = new SwRTFWriter(rFltName, rBaseURL);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
