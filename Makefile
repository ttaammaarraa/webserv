NAME		= webserv
CXX 		= c++
CXXFLAGS 	= -Wall -Wextra -Werror -std=c++98 -g
CPPFLAGS	= -Iinclude
SRC 		= main.cpp src/ConfigParser.cpp src/HttpRequest.cpp src/ResponseBuilder.cpp src/Server.cpp src/ServerUtils.cpp src/ChunkedBodyParser.cpp src/AutoIndexGenerator.cpp src/GetHandler.cpp src/PostDeleteHandler.cpp src/ResponseUtils.cpp src/CGIHandler.cpp
OBJ 		= $(SRC:.cpp=.o)



all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(NAME) $(OBJ)

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re