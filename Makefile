
NAME		= webserv
CXX 		= c++
CXXFLAGS 	= -Wall -Wextra -Werror -std=c++98
INCLUDES	= -I.
SRC 		= main.cpp ConfigParser.cpp HttpRequest.cpp FileHandler_tempsolve.cpp ResponseHandler_tempsolve.cpp Server.cpp
OBJ 		= $(SRC:.cpp=.o)


all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(NAME) $(OBJ)


clean:
	rm -f $(OBJ)


fclean: clean
	rm -f $(NAME)


re: fclean all


.PHONY: all clean fclean re