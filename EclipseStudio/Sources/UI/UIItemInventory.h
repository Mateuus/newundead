#pragma once

class r3dScaleformMovie;

class UIItemInventory
{
public:
	UIItemInventory();

	void initialize(r3dScaleformMovie* gfxMovie);

private:
	void addTabTypes();
	void addItems();
	void addCategories();

private:
	r3dScaleformMovie* gfxMovie_;
};