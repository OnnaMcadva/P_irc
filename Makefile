# Colors
DEF_COLOR   = "\033[0;39m"
GRAY        = "\033[0;90m"
YELLOW      = "\033[0;93m"
GREEN       = "\033[0;92m"
RED         = "\033[0;91m"
BLUE        = "\033[0;94m"
MAGENTA     = "\033[0;95m"
CYAN        = "\033[0;96m"
WHITE       = "\033[0;97m"
ORANGE      = "\033[38;5;222m"
GREEN_BR    = "\033[38;5;118m"
YELLOW_BR   = "\033[38;5;227m"
PINK_BR     = "\033[38;5;206m"
BLUE_BR     = "\033[38;5;051m"
PINK_BRR    = "\033[38;5;219m"

# Text styles
BOLD        = "\033[1m"
UNDERLINE   = "\033[4m"
BLINK       = "\033[5m"

NAME = ircserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -I.

SRCS = ircserv.cpp Server.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(NAME)
	@printf $(DEF_COLOR)
	@printf $(BOLD)$(YELLOW)"\nircserv compiled!\n\n"
	@printf $(BOLD)$(GREEN_BR)
	@printf "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣀⠀⠀⠀⠀\n"
	@printf "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣀⣤⣴⣶⡿⠿⠿⠿⠿⠿⠿⠿⢷\n"
	@printf "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣤⣶⣿⡿⠛⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
	@printf "⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⣿⣿⡿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
	@printf "⠀⠀⠀⠀⠀⠀⢀⣾⣿⡿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
	@printf "⠀⠀⠀⠀⠀⣰⣿⣿⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
	@printf "⠀⠀⠀⢠⣾⣿⠿⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
	@printf "⠀⠀⣴⣿⠿⠋⠀⠀⠀"$(BLUE)"Use ./ircserv 6667 jopa\n"
	@printf "⠀⠈⠉    "$(BLUE)"Use nc -C 127.0.0.1 6667\n\n"

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all
