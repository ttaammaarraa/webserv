NAME		= webserv
CXX 		= c++
CXXFLAGS 	= -Wall -Wextra -Werror -std=c++98
SRC 		= main.cpp ConfigParser.cpp HttpRequest.cpp ResponseBuilder.cpp Server.cpp ServerUtils.cpp
OBJ 		= $(SRC:.cpp=.o)

# For debug build: make DEBUG=1
ifeq ($(DEBUG),1)
	CXXFLAGS += -DDEBUG
endif

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJ)

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re 