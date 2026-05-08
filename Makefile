NAME		= webserv
CXX 		= c++
CXXFLAGS 	= -Wall -Wextra -Werror -std=c++98
CPPFLAGS	= -Iinclude
SRC 		= main.cpp src/ConfigParser.cpp src/HttpRequest.cpp src/ResponseBuilder.cpp src/Server.cpp src/ServerUtils.cpp src/ChunkedBodyParser.cpp src/AutoIndexGenerator.cpp src/GetHandler.cpp src/PostDeleteHandler.cpp src/ResponseUtils.cpp src/CGIHandler.cpp
OBJ 		= $(SRC:.cpp=.o)



TEST_BODY_NAME	= tests/http_request_body_test
TEST_PARSER_NAME	= tests/http_request_parser_test
TEST_CHUNKED_NAME	= tests/chunked_parser_test
TEST_AUTOINDEX_NAME	= tests/autoindex_test
TEST_BODY_OBJ	= tests/HttpRequestBodyTest.o src/HttpRequest.o src/ChunkedBodyParser.o
TEST_PARSER_OBJ	= tests/HttpRequestParserTest.o src/HttpRequest.o src/ChunkedBodyParser.o
TEST_CHUNKED_OBJ	= tests/ChunkedParserTest.o src/ChunkedBodyParser.o
TEST_AUTOINDEX_OBJ	= tests/AutoIndexTest.o src/HttpRequest.o src/ResponseBuilder.o src/Server.o src/ServerUtils.o src/ConfigParser.o src/ChunkedBodyParser.o src/AutoIndexGenerator.o src/GetHandler.o src/PostDeleteHandler.o src/ResponseUtils.o src/CGIHandler.o


all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(NAME) $(OBJ)

$(TEST_BODY_NAME): $(TEST_BODY_OBJ)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(TEST_BODY_NAME) $(TEST_BODY_OBJ)

$(TEST_PARSER_NAME): $(TEST_PARSER_OBJ)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(TEST_PARSER_NAME) $(TEST_PARSER_OBJ)

$(TEST_CHUNKED_NAME): $(TEST_CHUNKED_OBJ)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(TEST_CHUNKED_NAME) $(TEST_CHUNKED_OBJ)

$(TEST_AUTOINDEX_NAME): $(TEST_AUTOINDEX_OBJ)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(TEST_AUTOINDEX_NAME) $(TEST_AUTOINDEX_OBJ)

test: $(TEST_BODY_NAME) $(TEST_PARSER_NAME) $(TEST_CHUNKED_NAME) $(TEST_AUTOINDEX_NAME)
	./$(TEST_BODY_NAME)
	./$(TEST_PARSER_NAME)
	./$(TEST_CHUNKED_NAME)
	./$(TEST_AUTOINDEX_NAME)

clean:
	rm -f $(OBJ)
	rm -f $(TEST_BODY_OBJ) $(TEST_PARSER_OBJ) $(TEST_CHUNKED_OBJ) $(TEST_AUTOINDEX_OBJ)
	rm -f $(TEST_BODY_NAME) $(TEST_PARSER_NAME) $(TEST_CHUNKED_NAME) $(TEST_AUTOINDEX_NAME)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re test