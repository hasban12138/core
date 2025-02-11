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

#include <rtl/ustring.hxx>

namespace xmloff
{

    // properties
    #define PROPERTY_CLASSID "ClassId"
    #define PROPERTY_ECHOCHAR "EchoChar"
    #define PROPERTY_MULTILINE "MultiLine"
    #define PROPERTY_NAME "Name"
    #define PROPERTY_GRAPHIC "Graphic"
    #define PROPERTY_LABEL "Label"
    #define PROPERTY_TARGETFRAME "TargetFrame"
    #define PROPERTY_TARGETURL "TargetURL"
    #define PROPERTY_TITLE "Tag"
    #define PROPERTY_DROPDOWN "Dropdown"
    #define PROPERTY_PRINTABLE "Printable"
    #define PROPERTY_READONLY "ReadOnly"
    #define PROPERTY_DEFAULT_STATE "DefaultState"
    #define PROPERTY_TABSTOP "Tabstop"
    #define PROPERTY_STATE "State"
    #define PROPERTY_ENABLED "Enabled"
    #define PROPERTY_ENABLEVISIBLE "EnableVisible"
    #define PROPERTY_MAXTEXTLENGTH "MaxTextLen"
    #define PROPERTY_LINECOUNT "LineCount"
    #define PROPERTY_TABINDEX "TabIndex"
    #define PROPERTY_COMMAND "Command"
    #define PROPERTY_DATASOURCENAME "DataSourceName"
    #define PROPERTY_FILTER "Filter"
    #define PROPERTY_ORDER "Order"
    #define PROPERTY_ALLOWDELETES "AllowDeletes"
    #define PROPERTY_ALLOWINSERTS "AllowInserts"
    #define PROPERTY_ALLOWUPDATES "AllowUpdates"
    #define PROPERTY_APPLYFILTER "ApplyFilter"
    #define PROPERTY_ESCAPEPROCESSING "EscapeProcessing"
    #define PROPERTY_IGNORERESULT "IgnoreResult"
    #define PROPERTY_SUBMIT_ENCODING "SubmitEncoding"
    #define PROPERTY_SUBMIT_METHOD "SubmitMethod"
    #define PROPERTY_COMMAND_TYPE "CommandType"
    #define PROPERTY_NAVIGATION "NavigationBarMode"
    #define PROPERTY_CYCLE "Cycle"
    #define PROPERTY_BUTTONTYPE "ButtonType"
    #define PROPERTY_DATAFIELD "DataField"
    #define PROPERTY_BOUNDCOLUMN "BoundColumn"
    #define PROPERTY_EMPTY_IS_NULL "ConvertEmptyToNull"
    #define PROPERTY_INPUT_REQUIRED "InputRequired"
    #define PROPERTY_LISTSOURCE "ListSource"
    #define PROPERTY_LISTSOURCETYPE "ListSourceType"
    #define PROPERTY_ECHO_CHAR "EchoChar"
    #define PROPERTY_STRICTFORMAT "StrictFormat"
    #define PROPERTY_AUTOCOMPLETE "Autocomplete"
    #define PROPERTY_MULTISELECTION "MultiSelection"
    #define PROPERTY_DEFAULTBUTTON "DefaultButton"
    #define PROPERTY_TRISTATE "TriState"
    #define PROPERTY_CONTROLLABEL "LabelControl"
    #define PROPERTY_STRING_ITEM_LIST "StringItemList"
    #define PROPERTY_VALUE_SEQ "ValueItemList"
    #define PROPERTY_DEFAULT_SELECT_SEQ "DefaultSelection"
    #define PROPERTY_SELECT_SEQ "SelectedItems"
    #define PROPERTY_DATE_MIN "DateMin"
    #define PROPERTY_DATE_MAX "DateMax"
    #define PROPERTY_TIME_MIN "TimeMin"
    #define PROPERTY_TIME_MAX "TimeMax"
    #define PROPERTY_VALUE_MIN "ValueMin"
    #define PROPERTY_VALUE_MAX "ValueMax"
    #define PROPERTY_EFFECTIVE_MIN "EffectiveMin"
    #define PROPERTY_EFFECTIVE_MAX "EffectiveMax"
    #define PROPERTY_DEFAULT_DATE "DefaultDate"
    #define PROPERTY_DATE "Date"
    #define PROPERTY_DEFAULT_TIME "DefaultTime"
    #define PROPERTY_TIME "Time"
    #define PROPERTY_DEFAULT_VALUE "DefaultValue"
    #define PROPERTY_VALUE "Value"
    #define PROPERTY_HIDDEN_VALUE "HiddenValue"
    #define PROPERTY_DEFAULT_TEXT "DefaultText"
    #define PROPERTY_TEXT "Text"
    #define PROPERTY_EFFECTIVE_VALUE "EffectiveValue"
    #define PROPERTY_EFFECTIVE_DEFAULT "EffectiveDefault"
    #define PROPERTY_REFVALUE "RefValue"
    #define PROPERTY_URL "URL"
    #define PROPERTY_FONT "FontDescriptor"
    #define PROPERTY_BACKGROUNDCOLOR "BackgroundColor"
    #define PROPERTY_MASTERFIELDS "MasterFields"
    #define PROPERTY_DETAILFIELDS "DetailFields"
    #define PROPERTY_COLUMNSERVICENAME "ColumnServiceName"
    #define PROPERTY_FORMATKEY "FormatKey"
    #define PROPERTY_ALIGN "Align"
    #define PROPERTY_BORDER "Border"
    #define PROPERTY_AUTOCONTROLFOCUS "AutomaticControlFocus"
    #define PROPERTY_APPLYDESIGNMODE "ApplyFormDesignMode"
    #define PROPERTY_FORMATSSUPPLIER "FormatsSupplier"
    #define PROPERTY_LOCALE "Locale"
    #define PROPERTY_FORMATSTRING "FormatString"
    #define PROPERTY_DATEFORMAT "DateFormat"
    #define PROPERTY_TIMEFORMAT "TimeFormat"
    #define PROPERTY_PERSISTENCE_MAXTEXTLENGTH "PersistenceMaxTextLength"
    #define PROPERTY_SCROLLVALUE_MIN "ScrollValueMin"
    #define PROPERTY_SCROLLVALUE_MAX "ScrollValueMax"
    #define PROPERTY_SCROLLVALUE "ScrollValue"
    #define PROPERTY_SCROLLVALUE_DEFAULT "DefaultScrollValue"
    #define PROPERTY_LINE_INCREMENT "LineIncrement"
    #define PROPERTY_BLOCK_INCREMENT "BlockIncrement"
    #define PROPERTY_REPEAT_DELAY "RepeatDelay"
    #define PROPERTY_SPINVALUE "SpinValue"
    #define PROPERTY_SPINVALUE_MIN "SpinValueMin"
    #define PROPERTY_SPINVALUE_MAX "SpinValueMax"
    #define PROPERTY_DEFAULT_SPINVALUE "DefaultSpinValue"
    #define PROPERTY_SPIN_INCREMENT "SpinIncrement"
    #define PROPERTY_ORIENTATION "Orientation"
    #define PROPERTY_TOGGLE "Toggle"
    #define PROPERTY_FOCUS_ON_CLICK "FocusOnClick"
    #define PROPERTY_VISUAL_EFFECT "VisualEffect"
    #define PROPERTY_IMAGE_POSITION "ImagePosition"
    #define PROPERTY_IMAGE_ALIGN "ImageAlign"
    #define PROPERTY_SCALE_IMAGE "ScaleImage"
    #define PROPERTY_GROUP_NAME "GroupName"

    #define PROPERTY_BOUND_CELL "BoundCell"
    #define PROPERTY_LIST_CELL_RANGE "CellRange"
    #define PROPERTY_ADDRESS "Address"
    #define PROPERTY_FILE_REPRESENTATION "PersistentRepresentation"
    #define PROPERTY_RICH_TEXT "RichText"

    // services
    inline constexpr OUStringLiteral SERVICE_SPREADSHEET_DOCUMENT = u"com.sun.star.sheet.SpreadsheetDocument";
    inline constexpr OUStringLiteral SERVICE_CELLVALUEBINDING  = u"com.sun.star.table.CellValueBinding";
    inline constexpr OUStringLiteral SERVICE_LISTINDEXCELLBINDING  = u"com.sun.star.table.ListPositionCellBinding";
    inline constexpr OUStringLiteral SERVICE_CELLRANGELISTSOURCE = u"com.sun.star.table.CellRangeListSource";
    inline constexpr OUStringLiteral SERVICE_ADDRESS_CONVERSION  = u"com.sun.star.table.CellAddressConversion";
    inline constexpr OUStringLiteral SERVICE_RANGEADDRESS_CONVERSION = u"com.sun.star.table.CellRangeAddressConversion";

    // old service names (compatibility)
    #define SERVICE_PERSISTENT_COMPONENT_FORM "stardiv.one.form.component.Form"
    #define SERVICE_PERSISTENT_COMPONENT_EDIT "stardiv.one.form.component.Edit"
    #define SERVICE_PERSISTENT_COMPONENT_LISTBOX "stardiv.one.form.component.ListBox"
    #define SERVICE_PERSISTENT_COMPONENT_COMBOBOX "stardiv.one.form.component.ComboBox"
    #define SERVICE_PERSISTENT_COMPONENT_RADIOBUTTON "stardiv.one.form.component.RadioButton"
    #define SERVICE_PERSISTENT_COMPONENT_GROUPBOX "stardiv.one.form.component.GroupBox"
    #define SERVICE_PERSISTENT_COMPONENT_FIXEDTEXT "stardiv.one.form.component.FixedText"
    #define SERVICE_PERSISTENT_COMPONENT_COMMANDBUTTON "stardiv.one.form.component.CommandButton"
    #define SERVICE_PERSISTENT_COMPONENT_CHECKBOX "stardiv.one.form.component.CheckBox"
    #define SERVICE_PERSISTENT_COMPONENT_GRID "stardiv.one.form.component.Grid"
    #define SERVICE_PERSISTENT_COMPONENT_IMAGEBUTTON "stardiv.one.form.component.ImageButton"
    #define SERVICE_PERSISTENT_COMPONENT_FILECONTROL "stardiv.one.form.component.FileControl"
    #define SERVICE_PERSISTENT_COMPONENT_TIMEFIELD "stardiv.one.form.component.TimeField"
    #define SERVICE_PERSISTENT_COMPONENT_DATEFIELD "stardiv.one.form.component.DateField"
    #define SERVICE_PERSISTENT_COMPONENT_NUMERICFIELD "stardiv.one.form.component.NumericField"
    #define SERVICE_PERSISTENT_COMPONENT_CURRENCYFIELD "stardiv.one.form.component.CurrencyField"
    #define SERVICE_PERSISTENT_COMPONENT_PATTERNFIELD "stardiv.one.form.component.PatternField"
    #define SERVICE_PERSISTENT_COMPONENT_HIDDENCONTROL "stardiv.one.form.component.Hidden"
    #define SERVICE_PERSISTENT_COMPONENT_IMAGECONTROL "stardiv.one.form.component.ImageControl"
    #define SERVICE_PERSISTENT_COMPONENT_FORMATTEDFIELD "stardiv.one.form.component.FormattedField"

    // new service names, the old ones are translated into this new ones
    inline constexpr OUStringLiteral SERVICE_FORM = u"com.sun.star.form.component.Form";
    inline constexpr OUStringLiteral SERVICE_EDIT = u"com.sun.star.form.component.TextField";
    inline constexpr OUStringLiteral SERVICE_LISTBOX = u"com.sun.star.form.component.ListBox";
    inline constexpr OUStringLiteral SERVICE_COMBOBOX = u"com.sun.star.form.component.ComboBox";
    inline constexpr OUStringLiteral SERVICE_RADIOBUTTON = u"com.sun.star.form.component.RadioButton";
    inline constexpr OUStringLiteral SERVICE_GROUPBOX = u"com.sun.star.form.component.GroupBox";
    inline constexpr OUStringLiteral SERVICE_FIXEDTEXT = u"com.sun.star.form.component.FixedText";
    inline constexpr OUStringLiteral SERVICE_COMMANDBUTTON = u"com.sun.star.form.component.CommandButton";
    inline constexpr OUStringLiteral SERVICE_CHECKBOX = u"com.sun.star.form.component.CheckBox";
    inline constexpr OUStringLiteral SERVICE_GRID = u"com.sun.star.form.component.GridControl";
    inline constexpr OUStringLiteral SERVICE_IMAGEBUTTON = u"com.sun.star.form.component.ImageButton";
    inline constexpr OUStringLiteral SERVICE_FILECONTROL = u"com.sun.star.form.component.FileControl";
    inline constexpr OUStringLiteral SERVICE_TIMEFIELD = u"com.sun.star.form.component.TimeField";
    inline constexpr OUStringLiteral SERVICE_DATEFIELD = u"com.sun.star.form.component.DateField";
    inline constexpr OUStringLiteral SERVICE_NUMERICFIELD = u"com.sun.star.form.component.NumericField";
    inline constexpr OUStringLiteral SERVICE_CURRENCYFIELD = u"com.sun.star.form.component.CurrencyField";
    inline constexpr OUStringLiteral SERVICE_PATTERNFIELD = u"com.sun.star.form.component.PatternField";
    inline constexpr OUStringLiteral SERVICE_HIDDENCONTROL = u"com.sun.star.form.component.HiddenControl";
    inline constexpr OUStringLiteral SERVICE_IMAGECONTROL = u"com.sun.star.form.component.DatabaseImageControl";
    inline constexpr OUStringLiteral SERVICE_FORMATTEDFIELD = u"com.sun.star.form.component.FormattedField";

    // various strings
    #define EVENT_NAME_SEPARATOR "::"
    inline constexpr OUStringLiteral EVENT_TYPE = u"EventType";
    inline constexpr OUStringLiteral EVENT_LIBRARY = u"Library";
    inline constexpr OUStringLiteral EVENT_LOCALMACRONAME = u"MacroName";
    inline constexpr OUStringLiteral EVENT_SCRIPTURL = u"Script";
    inline constexpr OUStringLiteral EVENT_STAROFFICE = u"StarOffice";
    #define EVENT_STARBASIC "StarBasic"
    inline constexpr OUStringLiteral EVENT_APPLICATION = u"application";

}   // namespace xmloff

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
