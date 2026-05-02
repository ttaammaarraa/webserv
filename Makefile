NAME		= webserv
CXX 		= c++
CXXFLAGS 	= -Wall -Wextra -Werror -std=c++98
SRC 		= main.cpp ConfigParser.cpp HttpRequest.cpp ResponseBuilder.cpp Server.cpp ServerUtils.cpp
OBJ 		= $(SRC:.cpp=.o)

TEST_BODY_NAME		= tests/http_request_body_test
TEST_PARSER_NAME	= tests/http_request_parser_test
TEST_CHUNKED_NAME	= tests/http_request_chunked_test

TEST_BODY_SRC		= tests/HttpRequestBodyTest.cpp HttpRequest.cpp
TEST_PARSER_SRC		= tests/HttpRequestParserTest.cpp HttpRequest.cpp
TEST_CHUNKED_SRC	= tests/HttpRequestChunkedTest.cpp HttpRequest.cpp

# For debug build: make DEBUG=1
ifeq ($(DEBUG),1)
	CXXFLAGS += -DDEBUG
endif

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJ)

$(TEST_BODY_NAME): $(TEST_BODY_SRC)
	$(CXX) $(CXXFLAGS) -o $(TEST_BODY_NAME) $(TEST_BODY_SRC)

$(TEST_PARSER_NAME): $(TEST_PARSER_SRC)
	$(CXX) $(CXXFLAGS) -o $(TEST_PARSER_NAME) $(TEST_PARSER_SRC)

$(TEST_CHUNKED_NAME): $(TEST_CHUNKED_SRC)
	$(CXX) $(CXXFLAGS) -o $(TEST_CHUNKED_NAME) $(TEST_CHUNKED_SRC)

test: $(TEST_BODY_NAME) $(TEST_PARSER_NAME) $(TEST_CHUNKED_NAME)
	./$(TEST_BODY_NAME)
	./$(TEST_PARSER_NAME)
	./$(TEST_CHUNKED_NAME)

clean:
	rm -f $(OBJ)
	rm -f $(TEST_BODY_NAME) $(TEST_PARSER_NAME) $(TEST_CHUNKED_NAME)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re test
