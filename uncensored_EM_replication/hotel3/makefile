LIBS = 	-L/Users/johngao/work/thesis/CoinIpopt/build/lib		\
		-L/usr/local/lib										\
		-lcppad_ipopt -lipopt -llapack -lblas -ldl -lcoinhsl	\
		-L/usr/local/lib/gcc/5 									\
		-lgfortran -lm -lquadmath

INCS = 	-I/Users/johngao/work/thesis/CoinIpopt/build/include/coin/ThirdParty	\
		-I/Users/johngao/work/thesis/CoinIpopt/build/include 					\
		-I/Users/johngao/work/thesis/fast-cpp-csv-parser

EM_hotel_3.o: EM_hotel_3.cpp
	g++ -std=c++0x -c EM_hotel_3.cpp ${INCS}

EM_hotel_3: EM_hotel_3.o
	g++ -std=c++0x -o EM_hotel_3 EM_hotel_3.o ${LIBS}
