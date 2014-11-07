all:
	gcc -o rw_problem rw_problem.c -lpthread #-lrt
	./rw_problem
