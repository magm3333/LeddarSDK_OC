////////////////////////////////////////////////////////////////////////////////////////////////////
/// \file   Leddar/LdLjrRecorder.cpp
///
/// \brief  Implements the LdLjrRecorder class
///         A recorder using json lines format
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LdLjrRecorder.h"
#include "LdLjrDefines.h"

#include "LdPropertyIds.h"
#include "LtStringUtils.h"

#include "rapidjson/writer.h"

#include <ctime>
#include <cerrno>

////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn LeddarRecord::LdLjrRecorder::LdLjrRecorder( LeddarDevice::LdSensor *aSensor )
///
/// \brief  Constructor
///
/// \param [in] aSensor The sensor to record from.
///
/// \author David Levy
/// \date   October 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
LeddarRecord::LdLjrRecorder::LdLjrRecorder( LeddarDevice::LdSensor *aSensor ) : LdRecorder( aSensor ),
    mLastTimestamp( 0 )
{
    mStringBuffer = new rapidjson::StringBuffer;
    mWriter = new rapidjson::Writer<rapidjson::StringBuffer, rapidjson::UTF8<char>, rapidjson::UTF8<char>, rapidjson::CrtAllocator, 0>( *mStringBuffer );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn LeddarRecord::LdLjrRecorder::~LdLjrRecorder()
///
/// \brief  Destructor
///
/// \author David Levy
/// \date   October 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
LeddarRecord::LdLjrRecorder::~LdLjrRecorder()
{
    LdLjrRecorder::StopRecording();

    if( mStringBuffer != nullptr )
    {
        delete mStringBuffer;
        mStringBuffer = nullptr;
    }

    if( mWriter != nullptr )
    {
        delete mWriter;
        mWriter = nullptr;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn std::string LeddarRecord::LdLjrRecorder::StartRecording( const std::string &aPath )
///
/// \brief  Starts recording data from the sensor Create file headers / properties
///
/// \exception  std::invalid_argument   Raised when the file already exist.
/// \exception  std::logic_error        Raised when a a recording is already running.
/// \exception  std::runtime_error      Raised when a the file could not be created.
///
/// \param  aPath   (optional) Pathname of the record.
///
/// \return Pathname of the record.
///
/// \author David Levy
/// \date   October 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string LeddarRecord::LdLjrRecorder::StartRecording( const std::string &aPath )
{
    std::ifstream infile( aPath.c_str() ); //c_str for c++98

    if( infile.good() )
    {
        throw std::invalid_argument( "File already exist" );
    }

    if( mFile.is_open() )
    {
        throw std::logic_error( "Already recording" );
    }

    std::string lPath = "";

    if( aPath == "" )
    {
        std::time_t lTime = std::time( nullptr );
        char lStr[100];
        std::strftime( lStr, sizeof( lStr ), "%Y-%m-%d_%H-%M-%S", std::localtime( &lTime ) );

        if( mSensor->GetProperties()->FindProperty( LeddarCore::LdPropertyIds::ID_DEVICE_NAME ) != nullptr )
            lPath = mSensor->GetProperties()->GetTextProperty( LeddarCore::LdPropertyIds::ID_DEVICE_NAME )->GetStringValue() + "_" + std::string( lStr );
        else
            lPath = "UnknownDevice_" + std::string( lStr );
    }
    else
    {
        lPath = aPath;
    }

    if( lPath.length() > 4 && LeddarUtils::LtStringUtils::ToLower( lPath ).compare( lPath.length() - 4, 4, ".ljr" ) )
    {
        lPath += ".ljr";
    }

    mFile.open( lPath.c_str(), std::ios_base::out ); //c_str for c++98

    if( !mFile.is_open() )
    {
        throw std::runtime_error( "Could not create file - Error code: " + LeddarUtils::LtStringUtils::IntToString( errno ) );
    }

    AddFileHeader();
    AddAllProperties();
    return lPath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn void LeddarRecord::LdLjrRecorder::StopRecording()
///
/// \brief  Stops the recording (if any) - Called automatically when the object is destroyed
///
/// \author David Levy
/// \date   October 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
void LeddarRecord::LdLjrRecorder::StopRecording()
{
    if( !mWriter->IsComplete() && mStringBuffer->GetSize() != 0 ) //Last frame should still be open - Need to check size, because a reset mWriter is not complete
    {
        mWriter->EndObject(); //frame
        mWriter->EndObject(); //main object
        mFile << mStringBuffer->GetString() << std::endl;
    }

    if( mFile.is_open() )
    {
        mFile.close();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn void LeddarRecord::LdLjrRecorder::AddFileHeader()
///
/// \brief  Adds file header (first line of the file)
///
/// \author David Levy
/// \date   October 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
void LeddarRecord::LdLjrRecorder::AddFileHeader()
{
    mWriter->StartObject(); //Main object
    mWriter->Key( "header" );
    mWriter->StartObject(); //header object
    mWriter->Key( "prot_version" );
    mWriter->Uint( LeddarRecord::LJR_PROT_VERSION );
    mWriter->Key( "devicetype" );
    mWriter->Uint( mSensor->GetConnection()->GetDeviceType() );
    mWriter->Key( "protocol" );
    mWriter->Uint( mSensor->GetProperties()->GetIntegerProperty( LeddarCore::LdPropertyIds::ID_CONNECTION_TYPE )->ValueT<uint16_t>( 0 ) );
    mWriter->Key( "timestamp" );
    mWriter->Uint64( std::time( nullptr ) );
    mWriter->EndObject();//header object
    mWriter->EndObject(); //Main object

    if( !mWriter->IsComplete() )
    {
        throw std::logic_error( "invalid json" );
    }

    mFile << mStringBuffer->GetString() << std::endl;
    mStringBuffer->Clear();
    mWriter->Reset( *mStringBuffer );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn void LeddarRecord::LdLjrRecorder::AddAllProperties()
///
/// \brief  Adds all properties to the record. It should be the second line of the file
///
/// \author David Levy
/// \date   October 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
void LeddarRecord::LdLjrRecorder::AddAllProperties()
{
    mWriter->StartObject(); //Main object
    mWriter->Key( "prop" );
    mWriter->StartArray(); //Prop array

    std::vector<LeddarCore::LdProperty *> lProperties =  mSensor->GetProperties()->FindPropertiesByFeature( LeddarCore::LdProperty::F_SAVE );

    for( std::vector<LeddarCore::LdProperty *>::iterator lIter = lProperties.begin(); lIter != lProperties.end(); ++lIter )
    {
        LeddarCore::LdProperty *lProp = *lIter;

        if( lProp->Count() == 0 )
            continue;

        mWriter->StartObject(); // a single property
        mWriter->Key( "id" );
        mWriter->Uint( lProp->GetId() );

        if( lProp->GetType() == LeddarCore::LdProperty::TYPE_FLOAT )
        {
            mWriter->Key( "limits" );
            mWriter->StartArray();
            mWriter->Double( dynamic_cast<LeddarCore::LdFloatProperty *>( lProp )->MinValue() );
            mWriter->Double( dynamic_cast<LeddarCore::LdFloatProperty *>( lProp )->MaxValue() );
            mWriter->EndArray();
        }
        else if( lProp->GetType() == LeddarCore::LdProperty::TYPE_INTEGER )
        {
            mWriter->Key( "signed" );
            mWriter->Bool( dynamic_cast<LeddarCore::LdIntegerProperty *>( lProp )->Signed() );
            mWriter->Key( "limits" );
            mWriter->StartArray();
            mWriter->Int64( dynamic_cast<LeddarCore::LdIntegerProperty *>( lProp )->MinValue() );
            mWriter->Int64( dynamic_cast<LeddarCore::LdIntegerProperty *>( lProp )->MaxValue() );
            mWriter->EndArray();
        }
        else if( lProp->GetType() == LeddarCore::LdProperty::TYPE_ENUM )
        {
            LeddarCore::LdEnumProperty *lEnumProp = dynamic_cast<LeddarCore::LdEnumProperty *>( lProp );
            mWriter->Key( "enum" );
            mWriter->StartObject();

            for( size_t i = 0; i < lEnumProp->EnumSize(); ++i )
            {
                mWriter->Key( lEnumProp->EnumText( i ).c_str() );
                mWriter->Uint64( lEnumProp->EnumValue( i ) );
            }

            mWriter->EndObject();
        }

        AddPropertyValues( lProp );

        mWriter->EndObject(); // a single property
    }

    mWriter->EndArray(); //Prop array

    mWriter->EndObject(); //Main object

    if( !mWriter->IsComplete() )
    {
        throw std::logic_error( "invalid json" );
    }

    mFile << mStringBuffer->GetString() << std::endl;
    mStringBuffer->Clear();
    mWriter->Reset( *mStringBuffer );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn void LeddarRecord::LdLjrRecorder::AddPropertyValues( const LeddarCore::LdProperty *aProperty, rapidjson::Writer<rapidjson::StringBuffer> &aWriter )
///
/// \brief  Adds a property values to the current json line
///
/// \exception  std::logic_error    Raised when there is an unhandled property type.
///
/// \param          aProperty   The property to write to the Json. \param [in,out] aWriter     The Json writer.
///
/// \author David Levy
/// \date   October 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
void LeddarRecord::LdLjrRecorder::AddPropertyValues( const LeddarCore::LdProperty *aProperty )
{
    if( aProperty->Count() == 0 )
        return;

    mWriter->Key( "val" );

    if( aProperty->Count() > 1 )
    {
        mWriter->StartArray();
    }

    switch( aProperty->GetType() )
    {
        case LeddarCore::LdProperty::TYPE_BITFIELD:
            for( size_t i = 0; i < aProperty->Count(); ++i )
            {
                mWriter->Uint64( dynamic_cast<const LeddarCore::LdBitFieldProperty *>( aProperty )->Value( i ) );
            }

            break;

        case LeddarCore::LdProperty::TYPE_BOOL:
            for( size_t i = 0; i < aProperty->Count(); ++i )
            {
                mWriter->Bool( dynamic_cast<const LeddarCore::LdBoolProperty *>( aProperty )->Value( i ) );
            }

            break;

        case LeddarCore::LdProperty::TYPE_ENUM:
            for( size_t i = 0; i < aProperty->Count(); ++i )
            {
                mWriter->Uint64( dynamic_cast<const LeddarCore::LdEnumProperty *>( aProperty )->Value( i ) );
            }

            break;

        case LeddarCore::LdProperty::TYPE_FLOAT:
            for( size_t i = 0; i < aProperty->Count(); ++i )
            {
                mWriter->Double( dynamic_cast<const LeddarCore::LdFloatProperty *>( aProperty )->Value( i ) );
            }

            break;

        case LeddarCore::LdProperty::TYPE_INTEGER:
            for( size_t i = 0; i < aProperty->Count(); ++i )
            {
                if( dynamic_cast<const LeddarCore::LdIntegerProperty *>( aProperty )->Signed() )
                {
                    mWriter->Int64( dynamic_cast<const LeddarCore::LdIntegerProperty *>( aProperty )->ValueT<int32_t>( i ) );
                }
                else
                {
                    mWriter->Uint64( dynamic_cast<const LeddarCore::LdIntegerProperty *>( aProperty )->ValueT<uint32_t>( i ) );
                }
            }

            break;

        case LeddarCore::LdProperty::TYPE_TEXT:
            for( size_t i = 0; i < aProperty->Count(); ++i )
            {
                mWriter->String( dynamic_cast<const LeddarCore::LdTextProperty *>( aProperty )->GetStringValue( i ).c_str() );
            }

            break;

        case LeddarCore::LdProperty::TYPE_BUFFER:
            for( size_t i = 0; i < aProperty->Count(); ++i )
            {
                mWriter->String( dynamic_cast<const LeddarCore::LdBufferProperty *>( aProperty )->GetStringValue( i ).c_str() );
            }

            break;

        default:
            throw std::logic_error( "Unhandled property type" );
    }

    if( aProperty->Count() > 1 )
    {
        mWriter->EndArray();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn void LeddarRecord::LdLjrRecorder::AddProperty( const LeddarCore::LdProperty *aProperty, rapidjson::Writer<rapidjson::StringBuffer> &aWriter )
///
/// \brief  Adds a single property to the Json writer as an object
///
/// \param          aProperty   The property to save.
/// \param [in,out] aWriter     The writer.
///
/// \author David Levy
/// \date   October 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
void LeddarRecord::LdLjrRecorder::AddProperty( const LeddarCore::LdProperty *aProperty )
{
    mWriter->StartObject(); //a single property
    mWriter->Key( "id" );
    mWriter->Uint( aProperty->GetId() );

    AddPropertyValues( aProperty );

    mWriter->EndObject();//a single property
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn void LeddarRecord::LdLjrRecorder::Callback( LdObject *aSender, const SIGNALS aSignal, void * )
///
/// \brief  Callback function that handle new data or property change
///
/// \param [in]     aSender     The sender.
/// \param          aSignal     The type of the callback.
/// \param [in,out] parameter3  If non-null, additional parameters.
///
/// \author David Levy
/// \date   October 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
void LeddarRecord::LdLjrRecorder::Callback( LdObject *aSender, const SIGNALS aSignal, void * )
{
    if( !mFile.is_open() )
        return;

    if( aSignal == LeddarCore::LdObject::NEW_DATA )
    {
        if( aSender == mStates )
        {
            if( mStates->GetTimestamp() != mLastTimestamp )
            {
                if( mLastTimestamp != 0 )
                {
                    EndFrame();
                }

                StartFrame( mStates );
            }

            StatesCallback();
            mLastTimestamp = mStates->GetTimestamp();
        }
        else if( aSender == mEchoes )
        {
            if( mEchoes->GetTimestamp() != mLastTimestamp )
            {
                if( mLastTimestamp != 0 )
                {
                    EndFrame();
                }

                StartFrame( mEchoes );
            }

            EchoesCallback();
            mLastTimestamp = mEchoes->GetTimestamp();
        }
    }
    else if( aSignal == LeddarCore::LdObject::VALUE_CHANGED && dynamic_cast<LeddarCore::LdProperty *>( aSender ) != nullptr )
    {
        if( mLastTimestamp != 0 )
        {
            EndFrame();
        }

        PropertyCallback( dynamic_cast<LeddarCore::LdProperty *>( aSender ) );
        mLastTimestamp = 0; //To be sure we start a new frame without closing one
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn void LeddarRecord::LdLjrRecorder::StartFrame( LeddarConnection::LdResultProvider *aResults )
///
/// \brief  Starts a frame record
///
/// \param [in,out] aResults    Result provider for the timestamp
///
/// \author David Levy
/// \date   October 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
void LeddarRecord::LdLjrRecorder::StartFrame( LeddarConnection::LdResultProvider *aResults )
{
    mWriter->StartObject(); //main object
    mWriter->Key( "frame" );
    mWriter->StartObject(); //frame
    mWriter->Key( "ts" );
    mWriter->Uint( aResults->GetTimestamp() );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn void LeddarRecord::LdLjrRecorder::EndFrame()
///
/// \brief  Ends a frame record and output it to the file
///
/// \author David Levy
/// \date   October 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
void LeddarRecord::LdLjrRecorder::EndFrame()
{
    mWriter->EndObject(); //frame
    mWriter->EndObject(); //main object
    mFile << mStringBuffer->GetString() << std::endl;
    mStringBuffer->Clear();
    mWriter->Reset( *mStringBuffer );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn void LeddarRecord::LdLjrRecorder::StatesCallback()
///
/// \brief  Callback, called when there is new states
///         Append states to the record
///
/// \author David Levy
/// \date   October 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
void LeddarRecord::LdLjrRecorder::StatesCallback()
{
    mWriter->Key( "states" );
    mWriter->StartArray(); //states

    std::vector<LeddarCore::LdProperty *> lProperties =  mStates->GetProperties()->FindPropertiesByFeature( LeddarCore::LdProperty::F_SAVE );

    for( std::vector<LeddarCore::LdProperty *>::iterator lIter = lProperties.begin(); lIter != lProperties.end(); ++lIter )
    {
        if( ( *lIter )->Count() > 0 )
            AddProperty( *lIter );
    }

    mWriter->EndArray(); //states
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn void LeddarRecord::LdLjrRecorder::EchoesCallback()
///
/// \brief  Callback, called when there is new echoes
///         Append echoes to the record
///
/// \author David Levy
/// \date   October 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
void LeddarRecord::LdLjrRecorder::EchoesCallback()
{
    mWriter->Key( "echoes" );
    mWriter->StartArray(); //echoes

    mEchoes->Lock( LeddarConnection::B_GET );
    std::vector<LeddarConnection::LdEcho> &lEchoes = *( mEchoes->GetEchoes( LeddarConnection::B_GET ) );
    double lAmpScale = static_cast<double>( mEchoes->GetAmplitudeScale() );
    double lDistScale = static_cast<double>( mEchoes->GetDistanceScale() );

    for( size_t i = 0; i < mEchoes->GetEchoCount( LeddarConnection::B_GET ); ++i )
    {
        mWriter->StartArray(); //echo
        mWriter->Uint( lEchoes[i].mChannelIndex );
        mWriter->Double( lEchoes[i].mDistance / lDistScale );
        mWriter->Double( lEchoes[i].mAmplitude / lAmpScale );
        mWriter->Uint( lEchoes[i].mFlag );
        mWriter->EndArray(); //echo
    }

    mEchoes->UnLock( LeddarConnection::B_GET );

    mWriter->EndArray(); //echoes
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn void LeddarRecord::LdLjrRecorder::PropertyCallback()
///
/// \brief  Callback, called when a property is changed
///
/// \author David Levy
/// \date   October 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
void LeddarRecord::LdLjrRecorder::PropertyCallback( LeddarCore::LdProperty *aProperty )
{
    mWriter->StartObject(); //Main object
    mWriter->Key( "prop" );

    mWriter->StartArray(); //Prop array
    AddProperty( aProperty );
    mWriter->EndArray(); //Prop array

    mWriter->EndObject(); //Main object

    mFile << mStringBuffer->GetString() << std::endl;
    mStringBuffer->Clear();
    mWriter->Reset( *mStringBuffer );
}


