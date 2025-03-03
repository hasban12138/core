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

#include <ucbhelper/contentidentifier.hxx>
#include <com/sun/star/sdbc/SQLException.hpp>
#include <com/sun/star/uno/Reference.h>
#include <com/sun/star/beans/PropertyAttribute.hpp>
#include <ucbhelper/resultsetmetadata.hxx>
#include <cppuhelper/queryinterface.hxx>
#include "ftpresultsetbase.hxx"

using namespace ftp;
using namespace com::sun::star;

ResultSetBase::ResultSetBase(
    const uno::Reference< uno::XComponentContext >&  rxContext,
    const uno::Reference< ucb::XContentProvider >&  xProvider,
    const uno::Sequence< beans::Property >& seq )
    : m_xContext( rxContext ),
      m_xProvider( xProvider ),
      m_nRow( -1 ),
      m_nWasNull( true ),
      m_sProperty( seq )
{
}

ResultSetBase::~ResultSetBase()
{
}


// XInterface

void SAL_CALL
ResultSetBase::acquire()
    noexcept
{
    OWeakObject::acquire();
}


void SAL_CALL
ResultSetBase::release()
    noexcept
{
    OWeakObject::release();
}


uno::Any SAL_CALL
ResultSetBase::queryInterface( const uno::Type& rType )
{
    uno::Any aRet = cppu::queryInterface(
        rType,
        static_cast< lang::XComponent* >(this),
        static_cast< sdbc::XRow* >(this),
        static_cast< sdbc::XResultSet* >(this),
        static_cast< sdbc::XResultSetMetaDataSupplier* >(this),
        static_cast< beans::XPropertySet* >(this),
        static_cast< ucb::XContentAccess* >(this) );
    return aRet.hasValue() ? aRet : OWeakObject::queryInterface( rType );
}


// XComponent


void SAL_CALL
ResultSetBase::addEventListener(
    const uno::Reference< lang::XEventListener >& Listener )
{
    osl::MutexGuard aGuard( m_aMutex );

    if ( ! m_pDisposeEventListeners )
        m_pDisposeEventListeners.reset(
            new comphelper::OInterfaceContainerHelper3<lang::XEventListener>( m_aMutex ) );

    m_pDisposeEventListeners->addInterface( Listener );
}


void SAL_CALL
ResultSetBase::removeEventListener(
    const uno::Reference< lang::XEventListener >& Listener )
{
    osl::MutexGuard aGuard( m_aMutex );

    if ( m_pDisposeEventListeners )
        m_pDisposeEventListeners->removeInterface( Listener );
}


void SAL_CALL
ResultSetBase::dispose()
{
    osl::MutexGuard aGuard( m_aMutex );

    lang::EventObject aEvt;
    aEvt.Source = static_cast< lang::XComponent * >( this );

    if ( m_pDisposeEventListeners && m_pDisposeEventListeners->getLength() )
    {
        m_pDisposeEventListeners->disposeAndClear( aEvt );
    }
    if( m_pRowCountListeners && m_pRowCountListeners->getLength() )
    {
        m_pRowCountListeners->disposeAndClear( aEvt );
    }
    if( m_pIsFinalListeners && m_pIsFinalListeners->getLength() )
    {
        m_pIsFinalListeners->disposeAndClear( aEvt );
    }
}


//  XResultSet

sal_Bool SAL_CALL
ResultSetBase::next()
{
    ++m_nRow;
    return m_nRow < sal::static_int_cast<sal_Int32>(m_aItems.size());
}


sal_Bool SAL_CALL
ResultSetBase::isBeforeFirst()
{
    return m_nRow == -1;
}


sal_Bool SAL_CALL
ResultSetBase::isAfterLast()
{
    return m_nRow >= sal::static_int_cast<sal_Int32>(m_aItems.size());   // Cannot happen, if m_aFolder.isOpen()
}


sal_Bool SAL_CALL
ResultSetBase::isFirst()
{
    return m_nRow == 0;
}


sal_Bool SAL_CALL
ResultSetBase::isLast()
{
    if( m_nRow ==  sal::static_int_cast<sal_Int32>(m_aItems.size()) - 1 )
        return true;
    else
        return false;
}


void SAL_CALL
ResultSetBase::beforeFirst()
{
    m_nRow = -1;
}


void SAL_CALL
ResultSetBase::afterLast()
{
    m_nRow = m_aItems.size();
}


sal_Bool SAL_CALL
ResultSetBase::first()
{
    m_nRow = -1;
    return next();
}


sal_Bool SAL_CALL
ResultSetBase::last()
{
    m_nRow = m_aItems.size() - 1;
    return true;
}


sal_Int32 SAL_CALL
ResultSetBase::getRow()
{
    // Test, whether behind last row
    if( -1 == m_nRow || m_nRow >= sal::static_int_cast<sal_Int32>(m_aItems.size()) )
        return 0;
    else
        return m_nRow+1;
}


sal_Bool SAL_CALL ResultSetBase::absolute( sal_Int32 row )
{
    if( row >= 0 )
        m_nRow = row - 1;
    else
    {
        last();
        m_nRow += ( row + 1 );
        if( m_nRow < -1 )
            m_nRow = -1;
    }

    return 0<= m_nRow && m_nRow < sal::static_int_cast<sal_Int32>(m_aItems.size());
}


sal_Bool SAL_CALL
ResultSetBase::relative( sal_Int32 row )
{
    if( isAfterLast() || isBeforeFirst() )
        throw sdbc::SQLException();

    if( row > 0 )
        while( row-- )
            next();
    else if( row < 0 )
        while( row++ && m_nRow > - 1 )
            previous();

    return 0 <= m_nRow && m_nRow < sal::static_int_cast<sal_Int32>(m_aItems.size());
}


sal_Bool SAL_CALL
ResultSetBase::previous()
{
    if( m_nRow > sal::static_int_cast<sal_Int32>(m_aItems.size()) )
        m_nRow = m_aItems.size();  // Correct Handling of afterLast
    if( 0 <= m_nRow ) -- m_nRow;

    return 0 <= m_nRow && m_nRow < sal::static_int_cast<sal_Int32>(m_aItems.size());
}


void SAL_CALL
ResultSetBase::refreshRow()
{
}


sal_Bool SAL_CALL
ResultSetBase::rowUpdated()
{
    return false;
}

sal_Bool SAL_CALL
ResultSetBase::rowInserted()
{
    return false;
}

sal_Bool SAL_CALL
ResultSetBase::rowDeleted()
{
    return false;
}


uno::Reference< uno::XInterface > SAL_CALL
ResultSetBase::getStatement()
{
    return uno::Reference< uno::XInterface >();
}


// XCloseable

void SAL_CALL
ResultSetBase::close()
{
}


OUString SAL_CALL
ResultSetBase::queryContentIdentifierString()
{
    if( 0 <= m_nRow && m_nRow < sal::static_int_cast<sal_Int32>(m_aItems.size()) )
        return m_aPath[m_nRow];
    else
        return OUString();
}


uno::Reference< ucb::XContentIdentifier > SAL_CALL
ResultSetBase::queryContentIdentifier()
{
    if( 0 <= m_nRow && m_nRow < sal::static_int_cast<sal_Int32>(m_aItems.size()) )
    {
        if(!m_aIdents[m_nRow].is()) {
            OUString url = queryContentIdentifierString();
            if(!url.isEmpty() )
                m_aIdents[m_nRow] =
                    uno::Reference< ucb::XContentIdentifier >(
                        new ::ucbhelper::ContentIdentifier(url) );
        }
        return m_aIdents[m_nRow];
    }

    return uno::Reference<ucb::XContentIdentifier>();
}


uno::Reference< ucb::XContent > SAL_CALL
ResultSetBase::queryContent()
{
    if( 0 <= m_nRow && m_nRow < sal::static_int_cast<sal_Int32>(m_aItems.size()) )
        return m_xProvider->queryContent(queryContentIdentifier());
    else
        return uno::Reference< ucb::XContent >();
}

namespace {

class XPropertySetInfoImpl
    : public cppu::OWeakObject,
      public beans::XPropertySetInfo
{
public:

    explicit XPropertySetInfoImpl( const uno::Sequence< beans::Property >& aSeq )
        : m_aSeq( aSeq )
    {
    }

    void SAL_CALL acquire()
        noexcept override
    {
        OWeakObject::acquire();
    }


    void SAL_CALL release()
        noexcept override
    {
        OWeakObject::release();
    }

    uno::Any SAL_CALL queryInterface( const uno::Type& rType ) override
    {
        uno::Any aRet = cppu::queryInterface(
            rType,
            static_cast< beans::XPropertySetInfo* >(this) );
        return aRet.hasValue() ? aRet : OWeakObject::queryInterface( rType );
    }

    uno::Sequence< beans::Property > SAL_CALL getProperties() override
    {
        return m_aSeq;
    }

    beans::Property SAL_CALL getPropertyByName( const OUString& aName ) override
    {
        auto pProp = std::find_if(std::cbegin(m_aSeq), std::cend(m_aSeq),
            [&aName](const beans::Property& rProp) { return aName == rProp.Name; });
        if (pProp != std::cend(m_aSeq))
            return *pProp;
        throw beans::UnknownPropertyException(aName);
    }

    sal_Bool SAL_CALL hasPropertyByName( const OUString& Name ) override
    {
        return std::any_of(std::cbegin(m_aSeq), std::cend(m_aSeq),
            [&Name](const beans::Property& rProp) { return Name == rProp.Name; });
    }

private:

    uno::Sequence< beans::Property > m_aSeq;
};

}

// XPropertySet
uno::Reference< beans::XPropertySetInfo > SAL_CALL
ResultSetBase::getPropertySetInfo()
{
    uno::Sequence< beans::Property > seq
    {
        { "RowCount", -1, cppu::UnoType<sal_Int32>::get(), beans::PropertyAttribute::READONLY },
        { "IsRowCountFinal", -1, cppu::UnoType<sal_Bool>::get(), beans::PropertyAttribute::READONLY }
    };

    return uno::Reference< beans::XPropertySetInfo > (
        new XPropertySetInfoImpl( seq ) );
}


void SAL_CALL ResultSetBase::setPropertyValue(
    const OUString& aPropertyName, const uno::Any& /*aValue*/ )
{
    if( aPropertyName == "IsRowCountFinal" ||
        aPropertyName == "RowCount" )
        return;

    throw beans::UnknownPropertyException(aPropertyName);
}


uno::Any SAL_CALL ResultSetBase::getPropertyValue(
    const OUString& PropertyName )
{
    if( PropertyName == "IsRowCountFinal" )
    {
        return uno::Any(true);
    }
    else if ( PropertyName == "RowCount" )
    {
        sal_Int32 count = m_aItems.size();
        return uno::Any(count);
    }
    else
        throw beans::UnknownPropertyException(PropertyName);
}


void SAL_CALL ResultSetBase::addPropertyChangeListener(
    const OUString& aPropertyName,
    const uno::Reference< beans::XPropertyChangeListener >& xListener )
{
    if( aPropertyName == "IsRowCountFinal" )
    {
        osl::MutexGuard aGuard( m_aMutex );
        if ( ! m_pIsFinalListeners )
            m_pIsFinalListeners.reset(
                new comphelper::OInterfaceContainerHelper3<beans::XPropertyChangeListener>( m_aMutex ) );

        m_pIsFinalListeners->addInterface( xListener );
    }
    else if ( aPropertyName == "RowCount" )
    {
        osl::MutexGuard aGuard( m_aMutex );
        if ( ! m_pRowCountListeners )
            m_pRowCountListeners.reset(
                new comphelper::OInterfaceContainerHelper3<beans::XPropertyChangeListener>( m_aMutex ) );
        m_pRowCountListeners->addInterface( xListener );
    }
    else
        throw beans::UnknownPropertyException(aPropertyName);
}


void SAL_CALL ResultSetBase::removePropertyChangeListener(
    const OUString& aPropertyName,
    const uno::Reference< beans::XPropertyChangeListener >& aListener )
{
    if( aPropertyName == "IsRowCountFinal" &&
        m_pIsFinalListeners )
    {
        osl::MutexGuard aGuard( m_aMutex );
        m_pIsFinalListeners->removeInterface( aListener );
    }
    else if ( aPropertyName == "RowCount" &&
              m_pRowCountListeners )
    {
        osl::MutexGuard aGuard( m_aMutex );
        m_pRowCountListeners->removeInterface( aListener );
    }
    else
        throw beans::UnknownPropertyException(aPropertyName);
}


void SAL_CALL ResultSetBase::addVetoableChangeListener(
    const OUString& /*PropertyName*/,
    const uno::Reference< beans::XVetoableChangeListener >& /*aListener*/ )
{
}


void SAL_CALL ResultSetBase::removeVetoableChangeListener(
    const OUString& /*PropertyName*/,
    const uno::Reference< beans::XVetoableChangeListener >& /*aListener*/ )
{
}


// XResultSetMetaDataSupplier
uno::Reference< sdbc::XResultSetMetaData > SAL_CALL
ResultSetBase::getMetaData()
{
    return new ::ucbhelper::ResultSetMetaData( m_xContext, m_sProperty );
}


/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
