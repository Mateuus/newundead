#pragma once

#include "r3dFSBuilder.h"
#include "..\..\External\pugiXML\src\pugixml.hpp"

class BuilderConfig
{
  public:
	r3dFSBuilder&	bld_;
	
	bool		ParseNode(const pugi::xml_node& node);

  public:
	BuilderConfig(r3dFSBuilder& in_bld) : bld_(in_bld)
	{
	}

	bool		ParseConfig(const char* xml_fname);
};