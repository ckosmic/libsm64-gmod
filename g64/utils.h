#pragma once
#include <string>
#include <iostream>
#include <filesystem>
#include <Windows.h>

using namespace std;

string exe_path();
int version_compare(string v1, string v2);
void run_updater_script();