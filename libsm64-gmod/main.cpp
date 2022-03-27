#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "GarrysMod/Lua/Interface.h"
extern "C"
{
#include "libsm64.h"
}

using namespace std;
using namespace GarrysMod::Lua;

ILuaBase* GlobalLUA;

const int SM64_MAX_HEALTH = 8;

bool isGlobalInit = false;
uint8_t textureData[4 * SM64_TEXTURE_WIDTH * SM64_TEXTURE_HEIGHT];
double scaleFactor = 2.0;

void debug_print(char* text)
{
	GlobalLUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	GlobalLUA->GetField(-1, "print");
	GlobalLUA->PushString(text);
	GlobalLUA->Call(1, 0);
	GlobalLUA->Pop();
}

LUA_FUNCTION(IsGlobalInit)
{
	LUA->PushBool(isGlobalInit);
	return 1;
}

LUA_FUNCTION(GlobalInit)
{
	string inputFile = "C:\\Users\\ckosmic\\Desktop\\baserom.us.z64";
	ifstream infile(inputFile, ios::binary);
	infile.unsetf(ios::skipws);

	infile.seekg(0, ios::end);
	size_t size = infile.tellg();
	infile.seekg(0, ios::beg);

	vector<uint8_t> vec;
	vec.reserve(size);

	vec.insert(vec.begin(), istream_iterator<uint8_t>(infile), istream_iterator<uint8_t>());

	sm64_global_init(vec.data(), textureData, (SM64DebugPrintFunctionPtr)&debug_print);

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
	return 1;
}

LUA_FUNCTION(StaticSurfacesLoad)
{
	LUA->CheckType(1, Type::Table);

	size_t tableSize = LUA->ObjLen(-1);
	if (tableSize < 1 || tableSize % 3 != 0)
	{
		LUA->PushBool(false);
		return 1;
	}

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
		
		SM64Surface surface = { 0, 0, 0, 
			{
				{ vert1Pos.x * scaleFactor, vert1Pos.y * scaleFactor, vert1Pos.z * scaleFactor },
				{ vert2Pos.x * scaleFactor, vert2Pos.y * scaleFactor, vert2Pos.z * scaleFactor }, 
				{ vert3Pos.x * scaleFactor, vert3Pos.y * scaleFactor, vert3Pos.z * scaleFactor }} 
			};
		surfaces.push_back(surface);
	}
	LUA->Pop();

	sm64_static_surfaces_load(surfaces.data(), surfaces.size());

	LUA->PushBool(true);

	return 1;
}

LUA_FUNCTION(MarioCreate)
{
	LUA->CheckType(1, Type::Vector);
	Vector pos = LUA->GetVector(1);
	LUA->PushNumber((double)sm64_mario_create(pos.x * scaleFactor, pos.y * scaleFactor, pos.z * scaleFactor, 0, 0, 0));
	return 1;
}

LUA_FUNCTION(MarioTick)
{
	LUA->CheckType(1, Type::Number);
	LUA->CheckType(2, Type::Number);
	LUA->CheckType(3, Type::Number);
	LUA->CheckType(4, Type::Number);
	LUA->CheckType(5, Type::Number);
	LUA->CheckType(6, Type::Bool);
	LUA->CheckType(7, Type::Bool);
	LUA->CheckType(8, Type::Bool);

	SM64MarioInputs inputs;
	inputs.camLookX = LUA->GetNumber(2);
	inputs.camLookZ = LUA->GetNumber(3);
	inputs.stickX = LUA->GetNumber(4);
	inputs.stickY = LUA->GetNumber(5);
	inputs.buttonA = LUA->GetBool(6);
	inputs.buttonB = LUA->GetBool(7);
	inputs.buttonZ = LUA->GetBool(8);
	SM64MarioState outState;
	SM64MarioGeometryBuffers geoBuffers;
	geoBuffers.position = new float[8192]();
	geoBuffers.normal = new float[8192]();
	geoBuffers.color = new float[8192]();
	geoBuffers.uv = new float[8192]();

	sm64_mario_tick((int32_t)LUA->GetNumber(1), &inputs, &outState, &geoBuffers);

	LUA->CreateTable();
	
		LUA->PushNumber(1);
		LUA->CreateTable();
			Vector marioPos;
			marioPos.x = outState.position[0] / scaleFactor;
			marioPos.y = outState.position[2] / scaleFactor;
			marioPos.z = outState.position[1] / scaleFactor;
			LUA->PushNumber(1);
			LUA->PushVector(marioPos);
			LUA->SetTable(-3);

			Vector marioVel;
			marioVel.x = outState.velocity[0];
			marioVel.y = outState.velocity[1];
			marioVel.z = outState.velocity[2];
			LUA->PushNumber(2);
			LUA->PushVector(marioVel);
			LUA->SetTable(-3);

			LUA->PushNumber(3);
			LUA->PushNumber(outState.faceAngle);
			LUA->SetTable(-3);

			LUA->PushNumber(4);
			LUA->PushNumber(outState.health);
			LUA->SetTable(-3);
		LUA->SetTable(-3);

		LUA->PushNumber(2);
		LUA->CreateTable();
	
		for (int i = 0; i < geoBuffers.numTrianglesUsed*3; i++)
		{
			// Optimization? idk
			int iTimes3 = i * 3;
			int iTimes3Plus1 = iTimes3 + 1;
			int iTimes3Plus2 = iTimes3 + 2;

			LUA->PushNumber(i + 1);
			LUA->CreateTable();
				Vector pos;
				pos.x = geoBuffers.position[  iTimes3   ] / scaleFactor;
				pos.y = geoBuffers.position[iTimes3Plus2] / scaleFactor;
				pos.z = geoBuffers.position[iTimes3Plus1] / scaleFactor;
				LUA->PushNumber(1);
				LUA->PushVector(pos);
				LUA->SetTable(-3);

				Vector norm;
				norm.x = geoBuffers.normal[  iTimes3   ];
				norm.y = geoBuffers.normal[iTimes3Plus2];
				norm.z = geoBuffers.normal[iTimes3Plus1];
				LUA->PushNumber(2);
				LUA->PushVector(norm);
				LUA->SetTable(-3);

				float u = geoBuffers.uv[  i * 2  ] * 0.6875f;
				float v = geoBuffers.uv[i * 2 + 1];
				LUA->PushNumber(3);
				LUA->PushNumber(u);
				LUA->SetTable(-3);
				LUA->PushNumber(4);
				LUA->PushNumber(v);
				LUA->SetTable(-3);

				Vector col;
				col.x = geoBuffers.color[  iTimes3   ]*255;
				col.y = geoBuffers.color[iTimes3Plus1]*255;
				col.z = geoBuffers.color[iTimes3Plus2]*255;
				LUA->PushNumber(5);
				LUA->PushVector(col);
				LUA->SetTable(-3);
			LUA->SetTable(-3);
		}
		LUA->SetTable(-3);

	free(geoBuffers.position);
	geoBuffers.position = NULL;
	free(geoBuffers.normal);
	geoBuffers.normal = NULL;
	free(geoBuffers.color);
	geoBuffers.color = NULL;
	free(geoBuffers.uv);
	geoBuffers.uv = NULL;

	return 1;
}

LUA_FUNCTION(MarioDelete)
{
	LUA->CheckType(1, Type::Number);

	sm64_mario_delete((int32_t)LUA->GetNumber(1));
	
	return 1;
}

LUA_FUNCTION(SetMarioWaterLevel)
{
	LUA->CheckType(1, Type::Number);
	LUA->CheckType(2, Type::Number);

	sm64_set_mario_water_level((int32_t)LUA->GetNumber(1), (signed int)LUA->GetNumber(2));

	return 1;
}

LUA_FUNCTION(SetMarioPosition)
{
	LUA->CheckType(1, Type::Number);
	LUA->CheckType(2, Type::Vector);

	Vector pos = LUA->GetVector(2);
	sm64_set_mario_position((int32_t)LUA->GetNumber(1), (float)pos.x * scaleFactor, (float)pos.y * scaleFactor, (float)pos.z * scaleFactor);

	return 1;
}

LUA_FUNCTION(GetInputsFromButtonMask)
{
	LUA->CheckType(1, Type::Number);
	int buttons = LUA->GetNumber(1);

	Vector joystick;
	joystick.x = 0;
	joystick.y = 0;
	joystick.z = 0;
	bool jump = false;
	bool attack = false;
	bool crouch = false;

	if ((buttons & 8) == 8) { joystick.z--; }
	if ((buttons & 16) == 16) { joystick.z++; }
	if ((buttons & 1024) == 1024) { joystick.x--; }
	if ((buttons & 512) == 512) { joystick.x++; }
	if ((buttons & 2) == 2) { jump = true; }
	if ((buttons & 1) == 1) { attack = true; }
	if ((buttons & 4) == 4) { crouch = true; }

	// Normalize joystick inputs
	float magnitude = sqrt((joystick.x * joystick.x) + (joystick.z * joystick.z));
	if (magnitude > 0)
	{
		joystick.x /= magnitude;
		joystick.z /= magnitude;
	}

	LUA->CreateTable();
	LUA->PushNumber(1);
	LUA->PushVector(joystick);
	LUA->SetTable(-3);
	LUA->PushNumber(2);
	LUA->PushBool(jump);
	LUA->SetTable(-3);
	LUA->PushNumber(3);
	LUA->PushBool(attack);
	LUA->SetTable(-3);
	LUA->PushNumber(4);
	LUA->PushBool(crouch);
	LUA->SetTable(-3);

	return 1;
}

GMOD_MODULE_OPEN()
{
	GlobalLUA = LUA;

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->CreateTable();
		LUA->PushCFunction(GlobalInit);
		LUA->SetField(-2, "GlobalInit");
		LUA->PushCFunction(IsGlobalInit);
		LUA->SetField(-2, "IsGlobalInit");
		LUA->PushCFunction(GlobalTerminate);
		LUA->SetField(-2, "GlobalTerminate");
		LUA->PushCFunction(SetScaleFactor);
		LUA->SetField(-2, "SetScaleFactor");
		LUA->PushCFunction(StaticSurfacesLoad);
		LUA->SetField(-2, "StaticSurfacesLoad");
		LUA->PushCFunction(MarioCreate);
		LUA->SetField(-2, "MarioCreate");
		LUA->PushCFunction(MarioDelete);
		LUA->SetField(-2, "MarioDelete");
		LUA->PushCFunction(MarioTick);
		LUA->SetField(-2, "MarioTick");
		LUA->PushCFunction(SetMarioWaterLevel);
		LUA->SetField(-2, "SetMarioWaterLevel");
		LUA->PushCFunction(SetMarioPosition);
		LUA->SetField(-2, "SetMarioPosition");
		LUA->PushCFunction(GetInputsFromButtonMask);
		LUA->SetField(-2, "GetInputsFromButtonMask");
	LUA->SetField(-2, "libsm64");
	LUA->Pop();

	return 0;
}

GMOD_MODULE_CLOSE()
{
	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->PushNil();
	LUA->SetField(-2, "libsm64");
	LUA->Pop();

	sm64_global_terminate();
	isGlobalInit = false;
	return 0;
}