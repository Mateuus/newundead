#include "r3dPCH.h"
#include "r3d.h"

#include "BaseItemConfig.h"
#include "LangMngr.h"

bool BaseItemConfig::loadBaseFromXml(pugi::xml_node& xmlItem)
{
	r3d_assert(m_Description==NULL);
	r3d_assert(m_StoreIcon==NULL);
	r3d_assert(m_StoreName==NULL);
	r3d_assert(m_StoreNameW==NULL);
	r3d_assert(m_DescriptionW==NULL);

	category = (STORE_CATEGORIES)xmlItem.attribute("category").as_int();

	m_Weight = xmlItem.attribute("Weight").as_float()/1000.0f; // convert from grams into kg

	m_StoreIcon = strdup(xmlItem.child("Store").attribute("icon").value());

#ifndef WO_SERVER
	char tmpStr[64];
	sprintf(tmpStr, "%d_name", m_itemID);
	m_StoreNameW = wcsdup(gLangMngr.getString(tmpStr));
	m_StoreName = strdup(wideToUtf8(m_StoreNameW));
	sprintf(tmpStr, "%d_desc", m_itemID);
	m_DescriptionW = wcsdup(gLangMngr.getString(tmpStr));
	m_Description = strdup(wideToUtf8(m_DescriptionW));
#else
	const char* desc = xmlItem.child("Store").attribute("desc").value();
	r3d_assert(desc);
	m_Description = strdup(desc);
	m_StoreName = strdup(xmlItem.child("Store").attribute("name").value());
	m_StoreNameW = wcsdup(utf8ToWide(m_StoreName));
	m_DescriptionW = wcsdup(utf8ToWide(m_Description));
#endif

	/*FILE* tempFile = fopen_for_write("lang.txt", "ab");
	char tmpStr[2048];
	sprintf(tmpStr, "%d_name=%s\n", m_itemID, m_StoreName);
	fwrite(tmpStr, 1, strlen(tmpStr), tempFile);
	sprintf(tmpStr, "%d_desc=%s\n", m_itemID, m_Description);
	fwrite(tmpStr, 1, strlen(tmpStr), tempFile);
	fclose(tempFile);*/


	return true;
}

bool ModelItemConfig::loadBaseFromXml(pugi::xml_node& xmlItem)
{
	BaseItemConfig::loadBaseFromXml(xmlItem);
	r3d_assert(m_ModelPath==NULL);
	m_ModelPath = strdup(xmlItem.child("Model").attribute("file").value());

	return true;
}

bool LootBoxConfig::loadBaseFromXml(pugi::xml_node& xmlItem)
{
	BaseItemConfig::loadBaseFromXml(xmlItem);
	return true;
}

bool FoodConfig::loadBaseFromXml(pugi::xml_node& xmlItem)
{
	ModelItemConfig::loadBaseFromXml(xmlItem);
	Health = xmlItem.child("Property").attribute("health").as_float();
	Toxicity = xmlItem.child("Property").attribute("toxicity").as_float();
	Water= xmlItem.child("Property").attribute("water").as_float();
	Food = xmlItem.child("Property").attribute("food").as_float();
	Stamina = R3D_CLAMP(xmlItem.child("Property").attribute("stamina").as_float()/100.0f, 0.0f, 1.0f);

	m_ShopStackSize = xmlItem.child("Property").attribute("shopSS").as_int();

	return true;
}

