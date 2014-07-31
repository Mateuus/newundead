#include "r3dPCH.h"
#include "r3d.h"
#include "MeshPropertyLib.h"

MeshPropertyLib* g_MeshPropertyLib ;

//------------------------------------------------------------------------

MeshPropertyLibEntry::MeshPropertyLibEntry()
: DrawDistance( 0.f )
{

}

//------------------------------------------------------------------------

MeshPropertyLib::MeshPropertyLib()
{

}

//------------------------------------------------------------------------

MeshPropertyLib::~MeshPropertyLib()
{

}

//------------------------------------------------------------------------

MeshPropertyLib::Entry*
MeshPropertyLib::GetEntry( const string& key )
{
	Data::iterator found = mData.find( key );

	if( found != mData.end() )
		return &found->second ;
	else
		return NULL ;
}

//------------------------------------------------------------------------

#define MESHPROPERTY_FILE_NAME "MeshProperties.xml"

int
MeshPropertyLib::Load( const char* levelFolder )
{
	char fullPath[ 512 ];
	sprintf( fullPath, "%s/%s", levelFolder, MESHPROPERTY_FILE_NAME );

	r3dFile* f = r3d_open( fullPath, "rb" );
	if ( !f )
		return 0;

	char* fileBuffer = new char[f->size + 1];
	fread(fileBuffer, f->size, 1, f);
	fileBuffer[f->size] = 0;

	pugi::xml_document xmlFile;
	pugi::xml_parse_result parseResult = xmlFile.load_buffer_inplace(fileBuffer, f->size);

	fclose(f);

	if( !parseResult )
		r3dError("Failed to parse XML, error: %s", parseResult.description());

	pugi::xml_node xmlLib = xmlFile.child( "mesh_property_lib" );

	pugi::xml_node xmlEntry = xmlLib.child( "entry" );

	while( !xmlEntry.empty() )
	{
		Entry en;

		string name			= xmlEntry.attribute( "name" ).value();

		en.DrawDistance		= xmlEntry.attribute( "draw_distance" ).as_float();

		if( mData.find( name ) != mData.end() )
		{
			r3dError( "Duplicate entry %s in %s", name.c_str(), MESHPROPERTY_FILE_NAME );
		}

		mData[ name ] = en;

		xmlEntry = xmlEntry.next_sibling();
	}

	delete [] fileBuffer;

	return 1;
}

//------------------------------------------------------------------------

void MeshPropertyLib::Unload()
{
	mData.clear();
}

//------------------------------------------------------------------------

void
MeshPropertyLib::Save( const char* levelFolder ) const
{
	pugi::xml_document xmlFile;
	xmlFile.append_child( pugi::node_comment ).set_value("Mesh Property Lib");

	pugi::xml_node xmlLib = xmlFile.append_child();
	xmlLib.set_name( "mesh_property_lib" );

	for( Data::const_iterator i = mData.begin(), e = mData.end(); i != e; ++ i )
	{
		pugi::xml_node xmlEntry = xmlLib.append_child();

		xmlEntry.set_name( "entry" );

		const Entry& en = i->second;

		xmlEntry.append_attribute( "name"					)	= i->first.c_str();
		xmlEntry.append_attribute( "draw_distance"			)	= en.DrawDistance;
	}

	char fullPath[ 512 ];
	sprintf( fullPath, "%s/%s", levelFolder, MESHPROPERTY_FILE_NAME );

	xmlFile.save_file( fullPath );
}

//------------------------------------------------------------------------

void MeshPropertyLib::RemoveEntry( const string& key )
{
	Data::iterator found = mData.find( key );

	r3d_assert( found != mData.end() );

	mData.erase( found );
}

//------------------------------------------------------------------------

void MeshPropertyLib::RemoveEntry( const string& category, const string& meshName )
{
	RemoveEntry( ComposeKey( category, meshName ) );
}

//------------------------------------------------------------------------

void MeshPropertyLib::AddEntry( const string& key )
{
	r3d_assert( mData.find( key )  == mData.end() );

	mData.insert( Data::value_type( key, Entry() ) );
}

//------------------------------------------------------------------------

void MeshPropertyLib::AddEntry( const string& category, const string& meshName )
{
	AddEntry( ComposeKey( category, meshName ) );
}

//------------------------------------------------------------------------

bool MeshPropertyLib::Exists( const string& key )
{
	return mData.find( key ) != mData.end() ;
}

//------------------------------------------------------------------------

void MeshPropertyLib::GetEntryNames( stringlist_t* oNames ) const
{
	oNames->clear();

	for( Data::const_iterator i = mData.begin(), e = mData.end() ; i != e ; ++ i )
	{
		oNames->push_back( i->first );
	}
}

//------------------------------------------------------------------------

MeshPropertyLib::string MeshPropertyLib::ComposeKey( const string& category, const string& meshName )
{
	string res = category + ":" + meshName ;

	char* temp = (char*)_alloca( res.size() + 1 );

	r3d_assert( temp );

	strcpy( temp, res.c_str() );
	strlwr( temp );

	return temp ;
}

//------------------------------------------------------------------------

bool MeshPropertyLib::DecomposeKey( const string& key, string* oCat, string* oMeshName )
{
	size_t pos = key.find(":") ;
	if( pos != string::npos )
	{
		*oCat		= string( key.begin(), key.begin() + pos );
		*oMeshName	= string( key.begin() + pos + 1, key.end() );

		return true ;
	}

	return false ;
}

