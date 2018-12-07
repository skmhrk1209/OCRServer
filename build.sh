clang++ -std=c++14 server.cpp -o server \
-I/usr/local/Cellar/python@2/2.7.15_1/Frameworks/Python.framework/Versions/2.7/include/python2.7 \
-L/usr/local/Cellar/python@2/2.7.15_1/Frameworks/Python.framework/Versions/2.7/lib \
-lboost_system -lboost_regex -lpython2.7
