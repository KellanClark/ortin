
#ifndef ORTIN_NDS_HPP
#define ORTIN_NDS_HPP

#include "types.hpp"
#include "busshared.hpp"
#include "busarm9.hpp"
#include "busarm7.hpp"

class NDS {
public:
	BusShared shared;
	BusARM9 arm9;
	BusARM7 arm7;

	NDS();
	~NDS();
	void run();
};

#endif //ORTIN_NDS_HPP
