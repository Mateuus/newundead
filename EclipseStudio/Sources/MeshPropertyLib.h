#pragma once

struct MeshPropertyLibEntry
{
	float DrawDistance;

	MeshPropertyLibEntry();
};


class MeshPropertyLib
{
	// types
public:

	typedef MeshPropertyLibEntry Entry ;

	typedef std::string string;
	typedef std::map< string, Entry > Data ;
	typedef std::vector<std::string> stringlist_t;

	// 
public:
	MeshPropertyLib();
	~MeshPropertyLib();

	// access
public:
	Entry*			GetEntry( const string& key ) ;

	int				Load( const char* levelFolder );
	void			Unload();
	void			Save( const char* levelFolder ) const;

	void			RemoveEntry( const string& key );
	void			RemoveEntry( const string& category, const string& meshName );

	void			AddEntry( const string& key );
	void			AddEntry( const string& category, const string& meshName );

	bool			Exists( const string& key );

	void			GetEntryNames( stringlist_t* oNames ) const;

	static string	ComposeKey( const string& category, const string& meshName );
	static bool		DecomposeKey( const string& key, string* oCat, string* oMeshName );

	// data
private:
	Data mData;

} extern * g_MeshPropertyLib;

//------------------------------------------------------------------------


