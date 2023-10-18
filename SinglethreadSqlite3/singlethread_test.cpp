#include "pch.h"
#include "sqlite3.h"

#include<iostream>

namespace {
	TEST(SINGLETHREAD, THREADMODE) {
		std::cout << sqlite3_threadsafe() << std::endl;
	}
}