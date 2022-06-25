#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#if defined(_WIN32) && !defined(WIN32)
#define WIN32
#endif
#if defined(WIN32)
#include <Windows.h>
#include <direct.h>
#define GetCurrentDir _getcwd
#endif

#include "GarrysMod/Lua/Interface.h"
extern "C"
{
#include "libsm64.h"
}
#include "utils.h"

#define DEFINE_FUNCTION(x) GlobalLUA->PushCFunction(x); GlobalLUA->SetField(-2, #x);
#define PACKAGE_VERSION "0.1.1"

#define READ(a) f.read(reinterpret_cast<char*>(&a), sizeof(a));
#define READ_INTO(a, s) f.read(reinterpret_cast<char*>(&a), sizeof(a)); LUA->PushNumber(a); LUA->SetField(-2, s);

using namespace std;
using namespace GarrysMod::Lua;

ILuaBase* GlobalLUA;

const int SM64_MAX_HEALTH = 8;

float marioColorLUT[6][3] = {
	{ -1.0, 0.0, 0.0 },
	{ -1.0, 0.0, 0.0 },
	{ -1.0, 0.0, 0.0 },
	{ -1.0, 0.0, 0.0 },
	{ -1.0, 0.0, 0.0 },
	{ -1.0, 0.0, 0.0 },
};

struct mInfo {
	vector<int> references;
	int prevNumTris;
	int tickCount;
	SM64MarioGeometryBuffers geoBuffers;
	int animInfoRef = -1;
};

bool isGlobalInit = false;
uint8_t textureData[4 * SM64_TEXTURE_WIDTH * SM64_TEXTURE_HEIGHT];
double scaleFactor = 2.0;
map<int32_t, mInfo> mInfos;
bool autoUpdatesOn = false;
bool needsToUpdate = false;

void debug_print(char* text)
{
	GlobalLUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	GlobalLUA->GetField(-1, "print");
	GlobalLUA->PushString(text);
	GlobalLUA->Call(1, 0);
	GlobalLUA->Pop();
}

void debug_print(const char* text)
{
	GlobalLUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	GlobalLUA->GetField(-1, "print");
	GlobalLUA->PushString(text);
	GlobalLUA->Call(1, 0);
	GlobalLUA->Pop();
}

void debug_print(string text)
{
	GlobalLUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	GlobalLUA->GetField(-1, "print");
	GlobalLUA->PushString(text.c_str());
	GlobalLUA->Call(1, 0);
	GlobalLUA->Pop();
}

float fixAngle(float a) {
	return fmod(a + 180.0f, 360.0f) - 180.0f;
}

LUA_FUNCTION(GetPackageVersion)
{
	LUA->PushString(PACKAGE_VERSION);
	return 1;
}

LUA_FUNCTION(GetLibVersion)
{
	LUA->PushNumber(LIB_VERSION);
	return 1;
}

LUA_FUNCTION(SetAutoUpdateState)
{
	LUA->CheckType(1, Type::Bool);

	autoUpdatesOn = LUA->GetBool(1);
	LUA->Pop();

	return 1;
}

LUA_FUNCTION(CompareVersions)
{
	LUA->CheckType(1, Type::String); // Local version
	LUA->CheckType(2, Type::String); // Remote version

	const char* local = LUA->GetString(1);
	const char* remote = LUA->GetString(2);
	LUA->Pop(2);

	int result = version_compare(local, remote);
	if (result < 0)
	{
		// Local is out of date, needs to update
		needsToUpdate = true;
		LUA->PushNumber(0);
	}
	else if (result > 0)
	{
		// Local is over dated, how.
		needsToUpdate = false;
		LUA->PushNumber(1);
	}
	else
	{
		// Up to date
		needsToUpdate = false;
		LUA->PushNumber(2);
	}

	return 1;
}

LUA_FUNCTION(OpenFileDialog)
{
#if defined(WIN32)
	OPENFILENAME ofn = { 0 };
	TCHAR szFile[260] = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetActiveWindow();
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = "Z64 Files (.z64)\0*.z64\0";
	ofn.nFilterIndex = 0;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (GetOpenFileName(&ofn) == TRUE)
	{
		LUA->PushString((const char*)ofn.lpstrFile);
		return 1;
	}
	else {
		LUA->PushNumber(0);
		return 1;
	}
#endif
	LUA->PushNumber(-1);
	return 1;
}

vector<uint8_t> ReadBinaryFile(const char* fileName)
{
	ifstream infile(fileName, ios::binary);
	if (infile.fail() || !infile.is_open())
	{
		return vector<uint8_t>();
	}
	infile.unsetf(ios::skipws);

	infile.seekg(0, ios::end);
	size_t size = infile.tellg();
	infile.seekg(0, ios::beg);

	vector<uint8_t> vec;
	vec.reserve(size);

	vec.insert(vec.begin(), istream_iterator<uint8_t>(infile), istream_iterator<uint8_t>());

	infile.close();

	return vec;
}

LUA_FUNCTION(LoadMapCache)
{
	LUA->CheckType(1, Type::String); // Filename
	LUA->CheckType(2, Type::Number); // Filename

	const char* filename = LUA->GetString(1);
	uint32_t desiredVersion = LUA->GetNumber(2);
	char buff[FILENAME_MAX];
	GetCurrentDir(buff, FILENAME_MAX);
	string path = string(buff) + "\\garrysmod\\data\\" + string(filename);

	LUA->Pop(2);

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->GetField(-1, "libsm64");

	ifstream f(path.c_str(), ios::in | ios::binary);
	if (f.is_open())
	{
		uint32_t version;
		READ_INTO(version, "MapCacheVersion");
		if (version != desiredVersion)
		{
			LUA->PushBool(false);
			return 1;
		}
		int16_t xDelta;
		READ_INTO(xDelta, "XDelta");
		int16_t yDelta;
		READ_INTO(yDelta, "YDelta");
		int16_t x, y, z;
		READ(x); READ(y); READ(z);
		Vector worldMin;
		worldMin.x = x;
		worldMin.y = y;
		worldMin.z = z;
		LUA->PushVector(worldMin);
		LUA->SetField(-2, "WorldMin");
		READ(x); READ(y); READ(z);
		Vector worldMax;
		worldMax.x = x;
		worldMax.y = y;
		worldMax.z = z;
		LUA->PushVector(worldMax);
		LUA->SetField(-2, "WorldMax");
		uint16_t xChunks;
		READ_INTO(xChunks, "XChunks");
		uint16_t yChunks;
		READ_INTO(yChunks, "YChunks");
		uint16_t xDispChunks;
		READ_INTO(xDispChunks, "XDispChunks");
		uint16_t yDispChunks;
		READ_INTO(yDispChunks, "YDispChunks");

		// Map phys vertices table
		LUA->CreateTable();

		for (int x = 0; x < xChunks; x++)
		{
			LUA->PushNumber(x + 1);
			LUA->CreateTable();
			for (int y = 0; y < yChunks; y++)
			{
				uint32_t chunkSize;
				READ(chunkSize);
				LUA->PushNumber(y + 1);
				LUA->CreateTable();
				for (int i = 0; i < chunkSize; i++)
				{
					Vector vert;
					READ(vert.x); READ(vert.y); READ(vert.z);
					LUA->PushNumber(i + 1);
					LUA->PushVector(vert);
					LUA->SetTable(-3);
				}
				LUA->SetTable(-3);
			}
			LUA->SetTable(-3);
		}
		LUA->SetField(-2, "MapVertices");

		// Displacement vertices table
		LUA->CreateTable();

		for (int x = 0; x < xDispChunks; x++)
		{
			LUA->PushNumber(x + 1);
			LUA->CreateTable();
			for (int y = 0; y < yDispChunks; y++)
			{
				uint32_t chunkSize;
				READ(chunkSize);
				LUA->PushNumber(y + 1);
				LUA->CreateTable();
				for (int i = 0; i < chunkSize; i++)
				{
					Vector vert;
					READ(vert.x); READ(vert.y); READ(vert.z);
					LUA->PushNumber(i + 1);
					LUA->PushVector(vert);
					LUA->SetTable(-3);
				}
				LUA->SetTable(-3);
			}
			LUA->SetTable(-3);
		}
		LUA->SetField(-2, "DispVertices");
	}

	LUA->Pop();
	
	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION(IsGlobalInit)
{
	LUA->PushBool(isGlobalInit);
	return 1;
}

LUA_FUNCTION(GlobalInit)
{
	LUA->CheckType(1, Type::String);

	const char* path = LUA->GetString(1);
	LUA->Pop();

	vector<uint8_t> romFile = ReadBinaryFile(path);
	if (romFile.empty())
	{
		LUA->PushBool(false);
		return 1;
	}

	sm64_global_init(romFile.data(), textureData, (SM64DebugPrintFunctionPtr)&debug_print);

	LUA->CreateTable();
	for (int i = 0; i < sizeof(textureData); i += 4)
	{
		LUA->PushNumber(i / 4 + 1);
		LUA->CreateTable();
			LUA->PushNumber(1);
			LUA->PushNumber(textureData[i]);
			LUA->SetTable(-3);
			LUA->PushNumber(2);
			LUA->PushNumber(textureData[i+1]);
			LUA->SetTable(-3);
			LUA->PushNumber(3);
			LUA->PushNumber(textureData[i+2]);
			LUA->SetTable(-3);
			LUA->PushNumber(4);
			LUA->PushNumber(textureData[i+3]);
			LUA->SetTable(-3);
		LUA->SetTable(-3);
	}

	isGlobalInit = true;

	return 1;
}

LUA_FUNCTION(GlobalTerminate)
{
	sm64_global_terminate();
	isGlobalInit = false;

	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION(SetScaleFactor)
{
	LUA->CheckType(1, Type::Number);
	scaleFactor = LUA->GetNumber(1);
	LUA->Pop();
	return 1;
}

LUA_FUNCTION(StaticSurfacesLoad)
{
	LUA->CheckType(1, Type::Table);
	LUA->CheckType(1, Type::Table);

	size_t tableSize = LUA->ObjLen(-2);
	size_t dispTableSize = LUA->ObjLen(-1);
	if (tableSize < 1 || tableSize % 3 != 0)
	{
		LUA->PushBool(false);
		return 1;
	}

	vector<struct SM64Surface> surfaces;

	for (int i = 0; i < dispTableSize; i += 3)
	{
		LUA->PushNumber(i + 1);
		LUA->GetTable(-2);
		Vector vert1Pos = LUA->GetVector();
		LUA->Pop();
		LUA->PushNumber(i + 2);
		LUA->GetTable(-2);
		Vector vert2Pos = LUA->GetVector();
		LUA->Pop();
		LUA->PushNumber(i + 3);
		LUA->GetTable(-2);
		Vector vert3Pos = LUA->GetVector();
		LUA->Pop();

		SM64Surface surface = { 0, 0, 0,
			{
				{ -vert3Pos.x * scaleFactor, vert3Pos.z * scaleFactor, vert3Pos.y * scaleFactor },
				{ -vert2Pos.x * scaleFactor, vert2Pos.z * scaleFactor, vert2Pos.y * scaleFactor },
				{ -vert1Pos.x * scaleFactor, vert1Pos.z * scaleFactor, vert1Pos.y * scaleFactor }
			}
		};
		surfaces.push_back(surface);
	}
	LUA->Pop();

	for (int i = 0; i < tableSize; i += 3)
	{
		LUA->PushNumber(i + 1);
		LUA->GetTable(-2);
		Vector vert1Pos = LUA->GetVector();
		LUA->Pop();
		LUA->PushNumber(i + 2);
		LUA->GetTable(-2);
		Vector vert2Pos = LUA->GetVector();
		LUA->Pop();
		LUA->PushNumber(i + 3);
		LUA->GetTable(-2);
		Vector vert3Pos = LUA->GetVector();
		LUA->Pop();

		SM64Surface surface = { 0, 0, 0,
			{
				{ -vert3Pos.x * scaleFactor, vert3Pos.z * scaleFactor, vert3Pos.y * scaleFactor },
				{ -vert2Pos.x * scaleFactor, vert2Pos.z * scaleFactor, vert2Pos.y * scaleFactor },
				{ -vert1Pos.x * scaleFactor, vert1Pos.z * scaleFactor, vert1Pos.y * scaleFactor }
			}
		};
		surfaces.push_back(surface);
	}
	LUA->Pop();

	sm64_static_surfaces_load(surfaces.data(), surfaces.size());
	surfaces.clear();

	LUA->PushBool(true);

	return 1;
}

LUA_FUNCTION(SurfaceObjectCreate)
{
	LUA->CheckType(1, Type::Table);
	LUA->CheckType(2, Type::Vector);
	LUA->CheckType(3, Type::Angle);
	LUA->CheckType(4, Type::Number); // Surface type
	LUA->CheckType(5, Type::Number); // Terrain type

	uint16_t terrain = LUA->GetNumber(-1);
	int16_t type = LUA->GetNumber(-2);
	QAngle angle = LUA->GetAngle(-3);
	Vector position = LUA->GetVector(-4);
	size_t tableSize = LUA->ObjLen(-5);
	if (tableSize < 1 || tableSize % 3 != 0)
	{
		LUA->PushBool(false);
		return 1;
	}
	LUA->Pop(4);

	vector<struct SM64Surface> surfaces;

	for (int i = 0; i < tableSize; i += 3)
	{
		LUA->PushNumber(i + 1);
		LUA->GetTable(-2);
		Vector vert1Pos = LUA->GetVector();
		LUA->Pop();
		LUA->PushNumber(i + 2);
		LUA->GetTable(-2);
		Vector vert2Pos = LUA->GetVector();
		LUA->Pop();
		LUA->PushNumber(i + 3);
		LUA->GetTable(-2);
		Vector vert3Pos = LUA->GetVector();
		LUA->Pop();

		SM64Surface surface = {
			type, // Surface type
			0, // Force (unused?)
			terrain, // Terrain type
			{
				{ -vert3Pos.x * scaleFactor, vert3Pos.z * scaleFactor, vert3Pos.y * scaleFactor },
				{ -vert2Pos.x * scaleFactor, vert2Pos.z * scaleFactor, vert2Pos.y * scaleFactor },
				{ -vert1Pos.x * scaleFactor, vert1Pos.z * scaleFactor, vert1Pos.y * scaleFactor }
			}
		};
		surfaces.push_back(surface);
	}
	LUA->Pop();

	SM64ObjectTransform objTransform = {
		{ -position.x * scaleFactor, position.z * scaleFactor, position.y * scaleFactor },
		{ fixAngle(angle.z), fixAngle(-angle.y), fixAngle(-angle.x) }
	};

	SM64SurfaceObject surfObject;
	surfObject.transform = objTransform;
	surfObject.surfaceCount = surfaces.size();
	surfObject.surfaces = surfaces.data();

	uint32_t surfaceId = sm64_surface_object_create(&surfObject);

	LUA->PushNumber(surfaceId);

	return 1;
}

LUA_FUNCTION(SurfaceObjectMove)
{
	LUA->CheckType(1, Type::Number);
	LUA->CheckType(2, Type::Vector);
	LUA->CheckType(3, Type::Angle);

	uint32_t surfaceId = LUA->GetNumber(1);
	Vector position = LUA->GetVector(2);
	QAngle angle = LUA->GetAngle(3);

	LUA->Pop(3);

	SM64ObjectTransform objTransform = {
		{ -position.x * scaleFactor, position.z * scaleFactor, position.y * scaleFactor },
		{ fixAngle(angle.z), fixAngle(-angle.y), fixAngle(-angle.x) }
	};

	sm64_surface_object_move(surfaceId, &objTransform);

	LUA->PushBool(true);

	return 1;
}

LUA_FUNCTION(SurfaceObjectDelete)
{
	LUA->CheckType(1, Type::Number);

	uint32_t surfaceId = LUA->GetNumber(1);
	LUA->Pop();
	sm64_surface_object_delete(surfaceId);

	LUA->PushBool(true);

	return 1;
}

LUA_FUNCTION(ObjectCreate)
{
	LUA->CheckType(1, Type::Vector);
	LUA->CheckType(2, Type::Number);
	LUA->CheckType(3, Type::Number);

	Vector pos = LUA->GetVector(1);
	float height = LUA->GetNumber(2);
	float radius = LUA->GetNumber(3);
	LUA->Pop(3);

	SM64ObjectCollider* collider = (SM64ObjectCollider*)malloc(sizeof(SM64ObjectCollider));
	collider->position[0] = -pos.x * scaleFactor;
	collider->position[1] = pos.z * scaleFactor;
	collider->position[2] = pos.y * scaleFactor;
	collider->height = height * scaleFactor;
	collider->radius = radius * scaleFactor;

	uint32_t id = sm64_object_create(collider);

	LUA->PushNumber(id);

	return 1;
}

LUA_FUNCTION(ObjectMove)
{
	LUA->CheckType(1, Type::Number);
	LUA->CheckType(2, Type::Vector);

	uint32_t id = LUA->GetNumber(1);
	Vector pos = LUA->GetVector(2);
	LUA->Pop(2);

	sm64_object_move(id, -pos.x * scaleFactor, pos.z * scaleFactor, pos.y * scaleFactor);

	return 1;
}

LUA_FUNCTION(ObjectDelete)
{
	LUA->CheckType(1, Type::Number);

	uint32_t id = LUA->GetNumber(1);
	LUA->Pop();

	sm64_object_delete(id);

	return 1;
}

LUA_FUNCTION(MarioCreate)
{
	LUA->CheckType(1, Type::Vector);
	LUA->CheckType(2, Type::Bool);
	Vector pos = LUA->GetVector(1);
	bool isFake = LUA->GetBool(2);
	LUA->Pop(2);
	int32_t marioId = sm64_mario_create(-pos.x * scaleFactor, pos.z * scaleFactor, pos.y * scaleFactor, 0, 0, 0, isFake);
	LUA->PushNumber(marioId);

	// References:
	// 0: Vertex position table
	// 1: Vertex normal table
	// 2: Vertex U table
	// 3: Vertex V table
	// 4: Vertex color table
	// 5: Vertex info table (contains tables 0-4)
	// 6: State info table
	// 7: Tick result table ([1] == table 6, [2] == table 5)

	if (marioId >= 0)
	{
		mInfos[marioId].references = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
		mInfos[marioId].prevNumTris = -1;
		mInfos[marioId].tickCount = 0;

		for (int i = 0; i < mInfos[marioId].references.size(); i++)
		{
			LUA->CreateTable();
			mInfos[marioId].references[i] = LUA->ReferenceCreate();
		}

		LUA->CreateTable();
		mInfos[marioId].animInfoRef = LUA->ReferenceCreate(); // Anim info table

		mInfos[marioId].geoBuffers.position = new float[SM64_GEO_MAX_TRIANGLES * 3 * 3]();
		mInfos[marioId].geoBuffers.normal = new float[SM64_GEO_MAX_TRIANGLES * 3 * 3]();
		mInfos[marioId].geoBuffers.color = new float[SM64_GEO_MAX_TRIANGLES * 3 * 3]();
		mInfos[marioId].geoBuffers.uv = new float[SM64_GEO_MAX_TRIANGLES * 3 * 2]();
	}


	return 1;
}

void GenerateGeoTable(int32_t marioId, int refOffset, SM64MarioGeometryBuffers* geoBuffers, Vector offset)
{
	Vector pos;
	Vector norm;
	int k = 2;
	for (int i = 0; i < geoBuffers->numTrianglesUsed * 3; i++)
	{
		// Optimization? idk
		int l = (int)(i / 3) * 3 + k;
		int iTimes3 = l * 3;
		int iTimes3Plus1 = iTimes3 + 1;
		int iTimes3Plus2 = iTimes3 + 2;

		GlobalLUA->ReferencePush(mInfos[marioId].references[0 + refOffset]);
		pos.x = (-geoBuffers->position[iTimes3] + offset.x) / scaleFactor;
		pos.y = (geoBuffers->position[iTimes3Plus2] + offset.y) / scaleFactor;
		pos.z = (geoBuffers->position[iTimes3Plus1] + offset.z) / scaleFactor - 5;
		GlobalLUA->PushNumber(i + 1);
		GlobalLUA->PushVector(pos);
		GlobalLUA->SetTable(-3);
		GlobalLUA->Pop();

		GlobalLUA->ReferencePush(mInfos[marioId].references[1 + refOffset]);
		norm.x = -geoBuffers->normal[iTimes3];
		norm.y = geoBuffers->normal[iTimes3Plus2];
		norm.z = geoBuffers->normal[iTimes3Plus1];
		GlobalLUA->PushNumber(i + 1);
		GlobalLUA->PushVector(norm);
		GlobalLUA->SetTable(-3);
		GlobalLUA->Pop();

		GlobalLUA->ReferencePush(mInfos[marioId].references[2 + refOffset]);
		float u = geoBuffers->uv[l * 2] * 0.6875f;
		GlobalLUA->PushNumber(i + 1);
		GlobalLUA->PushNumber(u);
		GlobalLUA->SetTable(-3);
		GlobalLUA->Pop();

		GlobalLUA->ReferencePush(mInfos[marioId].references[3 + refOffset]);
		float v = geoBuffers->uv[l * 2 + 1];
		GlobalLUA->PushNumber(i + 1);
		GlobalLUA->PushNumber(v);
		GlobalLUA->SetTable(-3);
		GlobalLUA->Pop();

		int colorIndex = -1;
		if (mInfos[marioId].tickCount < 2)
		{
			for (int j = 0; j < 6; j++)
			{
				if (memcmp(&geoBuffers->color[iTimes3], marioColorLUT[j], 3 * sizeof(float)) == 0)
				{
					colorIndex = j;
					break;
				}

				if (marioColorLUT[j][0] == -1)
				{
					memcpy(marioColorLUT[j], &geoBuffers->color[iTimes3], 3 * sizeof(float));
					colorIndex = j;
					break;
				}
			}
		}
		else
		{
			for (int j = 0; j < 6; j++)
				if (memcmp(&geoBuffers->color[iTimes3], marioColorLUT[j], 3 * sizeof(float)) == 0)
				{
					colorIndex = j;
					break;
				}
		}

		GlobalLUA->ReferencePush(mInfos[marioId].references[4 + refOffset]);
		GlobalLUA->PushNumber(i + 1);
		GlobalLUA->PushNumber(colorIndex + 1);
		GlobalLUA->SetTable(-3);
		GlobalLUA->Pop();

		k--;
		if (k < 0) k = 2;
	}

	// Number of triangles changed, so empty the unused parts of the table or else lingering geometry will remain
	if (geoBuffers->numTrianglesUsed < mInfos[marioId].prevNumTris)
	{
		for (int b = 0; b < 2; b++)
		{
			int offset = b * 8;
			for (int i = geoBuffers->numTrianglesUsed * 3; i < mInfos[marioId].prevNumTris * 3; i++)
			{
				GlobalLUA->ReferencePush(mInfos[marioId].references[0 + offset]);
				GlobalLUA->PushNumber(i + 1);
				GlobalLUA->PushNil();
				GlobalLUA->SetTable(-3);
				GlobalLUA->Pop();

				GlobalLUA->ReferencePush(mInfos[marioId].references[1 + offset]);
				GlobalLUA->PushNumber(i + 1);
				GlobalLUA->PushNil();
				GlobalLUA->SetTable(-3);
				GlobalLUA->Pop();

				GlobalLUA->ReferencePush(mInfos[marioId].references[2 + offset]);
				GlobalLUA->PushNumber(i + 1);
				GlobalLUA->PushNil();
				GlobalLUA->SetTable(-3);
				GlobalLUA->Pop();

				GlobalLUA->ReferencePush(mInfos[marioId].references[3 + offset]);
				GlobalLUA->PushNumber(i + 1);
				GlobalLUA->PushNil();
				GlobalLUA->SetTable(-3);
				GlobalLUA->Pop();

				GlobalLUA->ReferencePush(mInfos[marioId].references[4 + offset]);
				GlobalLUA->PushNumber(i + 1);
				GlobalLUA->PushNil();
				GlobalLUA->SetTable(-3);
				GlobalLUA->Pop();
			}
		}
	}

	mInfos[marioId].prevNumTris = geoBuffers->numTrianglesUsed;
}

LUA_FUNCTION(GetMarioAnimInfo)
{
	LUA->CheckType(1, Type::Number); // Mario ID

	int32_t marioId = (int32_t)LUA->GetNumber(1);
	LUA->Pop();
	int16_t rot[3] = { 0, 0, 0 };

	SM64AnimInfo* animInfo = sm64_mario_get_anim_info(marioId, rot);

	LUA->ReferencePush(mInfos[marioId].animInfoRef);

	LUA->PushNumber(animInfo->animID);
	LUA->SetField(-2, "animID");
	LUA->PushNumber(animInfo->animYTrans);
	LUA->SetField(-2, "animYTrans");
	LUA->PushNumber(animInfo->animFrame);
	LUA->SetField(-2, "animFrame");
	LUA->PushNumber(animInfo->animTimer);
	LUA->SetField(-2, "animTimer");
	LUA->PushNumber(animInfo->animFrameAccelAssist);
	LUA->SetField(-2, "animFrameAccelAssist");
	LUA->PushNumber(animInfo->animAccel);
	LUA->SetField(-2, "animAccel");
	QAngle rotation;
	rotation.x = rot[0];
	rotation.y = rot[1];
	rotation.z = rot[2];
	LUA->PushAngle(rotation);
	LUA->SetField(-2, "rotation");

	return 1;
}

LUA_FUNCTION(MarioAnimTick)
{
	LUA->CheckType(1, Type::Table); // Animation info
	LUA->CheckType(2, Type::Number); // Mario ID
	LUA->CheckType(3, Type::Number); // Buffer index
	LUA->CheckType(4, Type::Number); // State flags
	LUA->CheckType(5, Type::Vector); // Vert offset

	Vector offset = LUA->GetVector(-1);
	uint32_t stateFlags = LUA->GetNumber(-2);
	int bufferIndex = (int)LUA->GetNumber(-3);
	int32_t marioId = (int32_t)LUA->GetNumber(-4);
	LUA->Pop(4);


	int refOffset = bufferIndex * 8;
	
	SM64AnimInfo animInfo;
	LUA->GetField(-1, "animID");
	animInfo.animID = LUA->GetNumber();
	LUA->Pop();
	LUA->GetField(-1, "animYTrans");
	animInfo.animYTrans = LUA->GetNumber();
	LUA->Pop();
	LUA->GetField(-1, "curAnim");
	animInfo.curAnim = (SM64Animation*)(uintptr_t)LUA->GetNumber();
	LUA->Pop();
	LUA->GetField(-1, "animFrame");
	animInfo.animFrame = LUA->GetNumber();
	LUA->Pop();
	LUA->GetField(-1, "animTimer");
	animInfo.animTimer = LUA->GetNumber();
	LUA->Pop();
	LUA->GetField(-1, "animFrameAccelAssist");
	animInfo.animFrameAccelAssist = LUA->GetNumber();
	LUA->Pop();
	LUA->GetField(-1, "animAccel");
	animInfo.animAccel = LUA->GetNumber();
	LUA->Pop();
	LUA->GetField(-1, "rotation");
	QAngle angle = LUA->GetAngle();
	LUA->Pop();
	
	LUA->Pop();


	SM64MarioState outState;

	int16_t rot[3] = { angle.x, angle.y, angle.z };

	sm64_mario_anim_tick(marioId, stateFlags, &animInfo, &mInfos[marioId].geoBuffers, rot);

	GenerateGeoTable(marioId, refOffset, &mInfos[marioId].geoBuffers, offset);

	LUA->ReferencePush(mInfos[marioId].references[5 + refOffset]); // Push vertex info table

		LUA->PushNumber(1);
		LUA->ReferencePush(mInfos[marioId].references[0 + refOffset]);
		LUA->SetTable(-3);
		LUA->PushNumber(2);
		LUA->ReferencePush(mInfos[marioId].references[1 + refOffset]);
		LUA->SetTable(-3);
		LUA->PushNumber(3);
		LUA->ReferencePush(mInfos[marioId].references[2 + refOffset]);
		LUA->SetTable(-3);
		LUA->PushNumber(4);
		LUA->ReferencePush(mInfos[marioId].references[3 + refOffset]);
		LUA->SetTable(-3);
		LUA->PushNumber(5);
		LUA->ReferencePush(mInfos[marioId].references[4 + refOffset]);
		LUA->SetTable(-3);

	mInfos[marioId].tickCount++;

	return 1;
}

LUA_FUNCTION(MarioTick)
{
	LUA->CheckType(1, Type::Number); // Mario ID
	LUA->CheckType(2, Type::Number); // Buffer index
	LUA->CheckType(3, Type::Vector); // Facing vector
	LUA->CheckType(4, Type::Vector); // Joystick vector
	LUA->CheckType(5, Type::Bool);   // A button
	LUA->CheckType(6, Type::Bool);   // B button
	LUA->CheckType(7, Type::Bool);   // Z button

	int32_t marioId = (int32_t)LUA->GetNumber(1);
	int bufferIndex = (int)LUA->GetNumber(2);
	int refOffset = bufferIndex * 8;

	Vector facing = LUA->GetVector(3);
	Vector joystick = LUA->GetVector(4);

	SM64MarioInputs inputs;
	inputs.camLookX = -facing.x;
	inputs.camLookZ = facing.y;
	inputs.stickX = joystick.x;
	inputs.stickY = joystick.z;
	inputs.buttonA = LUA->GetBool(5);
	inputs.buttonB = LUA->GetBool(6);
	inputs.buttonZ = LUA->GetBool(7);
	LUA->Pop(7);
	SM64MarioState outState;

	sm64_mario_tick(marioId, &inputs, &outState, &mInfos[marioId].geoBuffers);

	Vector offset;
	offset.x = outState.position[0];
	offset.y = -outState.position[2];
	offset.z = -outState.position[1];

	// Populate state info table
	LUA->ReferencePush(mInfos[marioId].references[6 + refOffset]);
		Vector marioPos;
		marioPos.x = -outState.position[0] / scaleFactor;
		marioPos.y = outState.position[2] / scaleFactor;
		marioPos.z = outState.position[1] / scaleFactor;
		LUA->PushNumber(1);
		LUA->PushVector(marioPos);
		LUA->SetTable(-3);

		Vector marioVel;
		marioVel.x = -outState.velocity[0];
		marioVel.y = outState.velocity[1];
		marioVel.z = outState.velocity[2];
		LUA->PushNumber(2);
		LUA->PushVector(marioVel);
		LUA->SetTable(-3);

		LUA->PushNumber(3);
		LUA->PushNumber(6.283185 - outState.faceAngle);
		LUA->SetTable(-3);

		LUA->PushNumber(4);
		LUA->PushNumber(outState.health);
		LUA->SetTable(-3);

		LUA->PushNumber(5);
		LUA->PushNumber(outState.action);
		LUA->SetTable(-3);

		LUA->PushNumber(6);
		LUA->PushNumber(outState.flags);
		LUA->SetTable(-3);

		LUA->PushNumber(7);
		LUA->PushNumber(outState.particleFlags);
		LUA->SetTable(-3);

		LUA->PushNumber(8);
		LUA->PushNumber(outState.invincTimer);
		LUA->SetTable(-3);
	LUA->Pop();

	// Populate geometry table
	GenerateGeoTable(marioId, refOffset, &mInfos[marioId].geoBuffers, offset);

	LUA->ReferencePush(mInfos[marioId].references[7 + refOffset]); // Push tick result table

	LUA->PushNumber(1);
	LUA->ReferencePush(mInfos[marioId].references[6 + refOffset]); // Push state info table
	LUA->SetTable(-3);

	LUA->PushNumber(2);
	LUA->ReferencePush(mInfos[marioId].references[5 + refOffset]); // Push vertex info table

		LUA->PushNumber(1);
		LUA->ReferencePush(mInfos[marioId].references[0 + refOffset]);
		LUA->SetTable(-3);
		LUA->PushNumber(2);
		LUA->ReferencePush(mInfos[marioId].references[1 + refOffset]);
		LUA->SetTable(-3);
		LUA->PushNumber(3);
		LUA->ReferencePush(mInfos[marioId].references[2 + refOffset]);
		LUA->SetTable(-3);
		LUA->PushNumber(4);
		LUA->ReferencePush(mInfos[marioId].references[3 + refOffset]);
		LUA->SetTable(-3);
		LUA->PushNumber(5);
		LUA->ReferencePush(mInfos[marioId].references[4 + refOffset]);
		LUA->SetTable(-3);

	LUA->SetTable(-3);

	mInfos[marioId].tickCount++;

	return 0;
}

LUA_FUNCTION(MarioDelete)
{
	LUA->CheckType(1, Type::Number);

	int32_t marioId = (int32_t)LUA->GetNumber(1);
	sm64_mario_delete(marioId);
	LUA->Pop();

	free(mInfos[marioId].geoBuffers.position);
	free(mInfos[marioId].geoBuffers.normal);
	free(mInfos[marioId].geoBuffers.color);
	free(mInfos[marioId].geoBuffers.uv);

	if (mInfos.count(marioId))
	{
		for (int i = 0; i < mInfos[marioId].references.size(); i++)
		{
			if (mInfos[marioId].references[i] > 0)
			{
				LUA->ReferenceFree(mInfos[marioId].references[i]);
			}
		}
		LUA->ReferenceFree(mInfos[marioId].animInfoRef);
		mInfos.erase(marioId);
	}
	
	return 1;
}

LUA_FUNCTION(SetMarioWaterLevel)
{
	LUA->CheckType(1, Type::Number);
	LUA->CheckType(2, Type::Number);

	sm64_set_mario_water_level((int32_t)LUA->GetNumber(1), (signed int)LUA->GetNumber(2));
	LUA->Pop(2);

	return 1;
}

LUA_FUNCTION(SetMarioInvincibility)
{
	LUA->CheckType(1, Type::Number);
	LUA->CheckType(2, Type::Number);

	sm64_set_mario_invincibility((int32_t)LUA->GetNumber(1), (int16_t)LUA->GetNumber(2));
	LUA->Pop(2);

	return 1;
}

LUA_FUNCTION(SetMarioPosition)
{
	LUA->CheckType(1, Type::Number);
	LUA->CheckType(2, Type::Vector);

	Vector pos = LUA->GetVector(2);
	sm64_set_mario_position((int32_t)LUA->GetNumber(1), -(float)pos.x * scaleFactor, (float)pos.z * scaleFactor, (float)pos.y * scaleFactor);
	LUA->Pop(2);

	return 1;
}

LUA_FUNCTION(SetMarioAngle)
{
	LUA->CheckType(1, Type::Number);
	LUA->CheckType(2, Type::Number);

	sm64_set_mario_angle((int32_t)LUA->GetNumber(1), (float)LUA->GetNumber(2));
	LUA->Pop(2);

	return 1;
}

LUA_FUNCTION(SetMarioAction)
{
	LUA->CheckType(1, Type::Number);
	LUA->CheckType(2, Type::Number);

	sm64_set_mario_action((int32_t)LUA->GetNumber(1), (uint32_t)LUA->GetNumber(2));
	LUA->Pop(2);

	return 1;
}

LUA_FUNCTION(SetMarioState)
{
	LUA->CheckType(1, Type::Number);
	LUA->CheckType(2, Type::Number);

	sm64_set_mario_state((int32_t)LUA->GetNumber(1), (uint32_t)LUA->GetNumber(2));
	LUA->Pop(2);

	return 1;
}

LUA_FUNCTION(SetMarioFloorOverrides)
{
	LUA->CheckType(1, Type::Number);
	LUA->CheckType(2, Type::Number); // Terrain type
	LUA->CheckType(3, Type::Number); // Floor type
	LUA->CheckType(4, Type::Number); // Floor force

	sm64_set_mario_floor_override((int32_t)LUA->GetNumber(1), (uint16_t)LUA->GetNumber(2), (int16_t)LUA->GetNumber(3), (int16_t)LUA->GetNumber(4));
	LUA->Pop(4);

	return 1;
}

LUA_FUNCTION(SetMarioHealth)
{
	LUA->CheckType(1, Type::Number);
	LUA->CheckType(2, Type::Number);

	sm64_set_mario_health((int32_t)LUA->GetNumber(1), (uint16_t)LUA->GetNumber(2));
	LUA->Pop(2);

	return 1;
}

LUA_FUNCTION(MarioTakeDamage)
{
	LUA->CheckType(1, Type::Number); // Mario ID
	LUA->CheckType(2, Type::Number); // Damage
	LUA->CheckType(3, Type::Number); // Subtype
	LUA->CheckType(4, Type::Vector); // Source Position

	Vector srcPos = LUA->GetVector(4);
	sm64_mario_take_damage((int32_t)LUA->GetNumber(1), (uint32_t)LUA->GetNumber(2), (uint32_t)LUA->GetNumber(3), -srcPos.x * scaleFactor, srcPos.z * scaleFactor, srcPos.y * scaleFactor);
	LUA->Pop(4);

	return 1;
}

LUA_FUNCTION(MarioHeal)
{
	LUA->CheckType(1, Type::Number); // Mario ID
	LUA->CheckType(2, Type::Number); // Heal counter

	sm64_mario_heal((int32_t)LUA->GetNumber(1), (uint8_t)LUA->GetNumber(2));
	LUA->Pop(2);

	return 1;
}

LUA_FUNCTION(MarioEnableCap)
{
	LUA->CheckType(1, Type::Number); // Mario ID
	LUA->CheckType(2, Type::Number); // Cap flag
	LUA->CheckType(3, Type::Number); // Cap timer
	LUA->CheckType(4, Type::Bool); // Cap timer

	sm64_mario_interact_cap((int32_t)LUA->GetNumber(1), (uint32_t)LUA->GetNumber(2), (uint16_t)LUA->GetNumber(3), LUA->GetBool(4));
	LUA->Pop(4);

	return 1;
}

LUA_FUNCTION(MarioExtendCapTime)
{
	LUA->CheckType(1, Type::Number); // Mario ID
	LUA->CheckType(2, Type::Number); // Cap time

	sm64_mario_extend_cap((int32_t)LUA->GetNumber(1), (uint16_t)LUA->GetNumber(2));
	LUA->Pop(2);

	return 1;
}

LUA_FUNCTION(MarioAttack)
{
	LUA->CheckType(1, Type::Number); // Mario ID
	LUA->CheckType(2, Type::Vector); // Enemy position
	LUA->CheckType(3, Type::Number); // Hitbox height

	Vector pos = LUA->GetVector(2);

	bool succeeded = sm64_mario_attack((int32_t)LUA->GetNumber(1), -pos.x * scaleFactor, pos.z * scaleFactor, pos.y * scaleFactor, (float)LUA->GetNumber(3) * scaleFactor);
	LUA->Pop(3);
	LUA->PushBool(succeeded);

	return 1;
}

LUA_FUNCTION(GetMarioTableReference)
{
	LUA->CheckType(1, Type::Number); // Mario ID
	LUA->CheckType(2, Type::Number); // Reference index

	int32_t marioId = LUA->GetNumber(1);
	int refInd = LUA->GetNumber(2);
	LUA->Pop(2);

	LUA->ReferencePush(mInfos[marioId].references[refInd]);

	return 1;
}

LUA_FUNCTION(GetSoundArg)
{
	LUA->CheckType(1, Type::Number); // Bank
	LUA->CheckType(2, Type::Number); // Play flags
	LUA->CheckType(3, Type::Number); // Sound ID
	LUA->CheckType(4, Type::Number); // Priority
	LUA->CheckType(5, Type::Number); // Flags2

	uint32_t soundArg = sm64_get_sound_arg((uint32_t)LUA->GetNumber(1), (uint32_t)LUA->GetNumber(2), (uint32_t)LUA->GetNumber(3), (uint32_t)LUA->GetNumber(4), (uint32_t)LUA->GetNumber(5));
	LUA->Pop(5);
	LUA->PushNumber(soundArg);

	return 1;
}

LUA_FUNCTION(PlaySoundGlobal)
{
	LUA->CheckType(1, Type::Number); // Sound bits

	sm64_play_sound_global((int32_t)LUA->GetNumber(1));
	LUA->Pop(1);

	return 1;
}

LUA_FUNCTION(PlayMusic)
{
	LUA->CheckType(1, Type::Number); // Player
	LUA->CheckType(2, Type::Number); // Seq Args
	LUA->CheckType(3, Type::Number); // Fade timer

	sm64_play_music((uint8_t)LUA->GetNumber(1), (uint16_t)LUA->GetNumber(2), (uint16_t)LUA->GetNumber(3));
	LUA->Pop(3);

	return 1;
}

LUA_FUNCTION(StopMusic)
{
	LUA->CheckType(1, Type::Number); // Seq ID

	sm64_stop_background_music((uint16_t)LUA->GetNumber(1));
	LUA->Pop(1);

	return 1;
}

LUA_FUNCTION(GetCurrentMusic)
{
	LUA->PushNumber(sm64_get_current_background_music());

	return 1;
}

LUA_FUNCTION(SetGlobalVolume)
{
	LUA->CheckType(1, Type::Number); // Volume

	sm64_set_volume((float)LUA->GetNumber(1));
	LUA->Pop(1);

	return 1;
}

GMOD_MODULE_OPEN()
{
	GlobalLUA = LUA;

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->CreateTable();
		DEFINE_FUNCTION(GlobalInit);
		DEFINE_FUNCTION(IsGlobalInit);
		DEFINE_FUNCTION(GlobalTerminate);
		DEFINE_FUNCTION(SetScaleFactor);
		DEFINE_FUNCTION(StaticSurfacesLoad);
		DEFINE_FUNCTION(MarioCreate);
		DEFINE_FUNCTION(MarioDelete);
		DEFINE_FUNCTION(MarioTick);
		DEFINE_FUNCTION(MarioAnimTick);
		DEFINE_FUNCTION(SetMarioWaterLevel);
		DEFINE_FUNCTION(SetMarioInvincibility);
		DEFINE_FUNCTION(SetMarioPosition);
		DEFINE_FUNCTION(SetMarioAngle);
		DEFINE_FUNCTION(SetMarioAction);
		DEFINE_FUNCTION(SetMarioState);
		DEFINE_FUNCTION(SetMarioFloorOverrides);
		DEFINE_FUNCTION(SetMarioHealth);
		DEFINE_FUNCTION(SurfaceObjectCreate);
		DEFINE_FUNCTION(SurfaceObjectMove);
		DEFINE_FUNCTION(SurfaceObjectDelete);
		DEFINE_FUNCTION(ObjectCreate);
		DEFINE_FUNCTION(ObjectMove);
		DEFINE_FUNCTION(ObjectDelete);
		DEFINE_FUNCTION(MarioTakeDamage);
		DEFINE_FUNCTION(MarioHeal);
		DEFINE_FUNCTION(MarioEnableCap);
		DEFINE_FUNCTION(MarioExtendCapTime);
		DEFINE_FUNCTION(MarioAttack);
		DEFINE_FUNCTION(GetMarioAnimInfo);
		DEFINE_FUNCTION(GetMarioTableReference);
		DEFINE_FUNCTION(OpenFileDialog);
		DEFINE_FUNCTION(GetSoundArg);
		DEFINE_FUNCTION(PlaySoundGlobal);
		DEFINE_FUNCTION(PlayMusic);
		DEFINE_FUNCTION(StopMusic);
		DEFINE_FUNCTION(GetCurrentMusic);
		DEFINE_FUNCTION(GetLibVersion);
		DEFINE_FUNCTION(GetPackageVersion);
		DEFINE_FUNCTION(SetAutoUpdateState);
		DEFINE_FUNCTION(CompareVersions);
		DEFINE_FUNCTION(SetGlobalVolume);
		DEFINE_FUNCTION(LoadMapCache);
	LUA->SetField(-2, "libsm64");
	LUA->Pop();

	return 0;
}

GMOD_MODULE_CLOSE()
{
	sm64_global_terminate();
	isGlobalInit = false;

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->PushNil();
	LUA->SetField(-2, "libsm64");
	LUA->Pop();

	if(autoUpdatesOn && needsToUpdate)
		run_updater_script();

	return 0;
}