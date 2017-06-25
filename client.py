#!/usr/bin/env python
import comm
import math
import pygame
from time import sleep
from pygame.locals import *
from sys import argv, exit
import os

RELOAD_TIME = 3000 # in milisseconds

# Colors used to draw
BACKGROUND = (24, 42, 60)
PLAYER_1 = (39, 174, 96)
PLAYER_2 = (192, 57, 43)
CANNON_1 = (85, 60, 74)
CANNON_2 = (60, 86, 69)
SHOT1 = (0, 255, 0)
SHOT2 = (255, 0, 0)
WHITE = (255, 255, 255)
RELOAD_BAR = (190, 190, 190)

LEFT = 0
DOWN = 1
RIGHT = 2
UP = 3
MOUSE = 4

class Player():
    def __init__(self, curr_hp, startpos):
        self.curr_hp = curr_hp

        self.alive = True

        self.pos = startpos

    def updatePos(self, new_pos):
        self.pos = new_pos

class Shot():
    def __init__(self):
        self.active = False
        self.pos = (0, 0)

    def setActive(self, active):
        self.active = active

    def setPos(self, new_pos):
        self.pos = new_pos

class GameState():
    
    def __init__(self):
        self.players = [Player(3, (100, 100)), Player(3, (100, 200))]
        self.shots = [Shot(), Shot()]

   # def addPlayer(self, pid, pos):
   #     self.players[pid] = pos

    def updatePlayerPos(self, player, new_pos):
        self.players[player].updatePos(new_pos)    

    # breaks down info and updates shot position
    def updateShotsPos(self, shots_info):
        s0_info = shots_info[:3]
        s1_info = shots_info[3:]

        if s0_info[0] == 1:
            self.shots[0].setActive(True)
        else:
            self.shots[0].setActive(False)
        self.shots[0].setPos(s0_info[1:])

        if s1_info[0] == 1:
            self.shots[1].setActive(True)
        else:
            self.shots[1].setActive(False)
        self.shots[1].setPos(s1_info[1:])

class ShooterGame():

    def __init__(self, port, player_id):
        pygame.init()
    
        #self.reloadFlag = False # flag used when drawing reloading meter

        self.size = [800, 600]
        self.font = pygame.font.Font(None, 30)

        self.resetFlag = False

        self.player_id = player_id
        self.p0_score = 0
        self.p1_score = 0

        self.degrees = 0.0

        self.screen = pygame.display.set_mode(self.size)
        self.loadImages() # load sprites used in-game

        # connect to server
        comm.clientConnect(port)

        self.state = GameState()
        self.pressedKeys = [False, False, False, False, False] # keeps track when a key is pressed and hold

    def loadImages(self):
        self.player0_img = pygame.image.load(os.path.join('Sprites', 'player1.png'))
        self.player1_img = pygame.image.load(os.path.join('Sprites', 'player2.png'))
        img_size = self.player0_img.get_rect().size # getting size
        self.drawPlayerOffset = img_size[0]/2 # sprites are squared so it doesn't matter if we use x or y

        self.cannon0_img = pygame.image.load(os.path.join('Sprites', 'cannon1.png'))
        self.cannon1_img = pygame.image.load(os.path.join('Sprites', 'cannon2.png'))
        self.cannonYOffset = 25 # olhometro pegando fooogo
        self.cannonXOffset = 9

    def checkForKeyPress(self):
        self.mouse_pos = pygame.mouse.get_pos()
        for event in pygame.event.get():
            if event.type == QUIT:
                exit(0)

            if event.type == pygame.MOUSEBUTTONDOWN:
                self.pressedKeys[MOUSE] = True

            if event.type == KEYDOWN:
                if event.key == K_a:
                    self.pressedKeys[LEFT] = True
                if event.key == K_d:
                    self.pressedKeys[RIGHT] = True
                if event.key == K_w:
                    self.pressedKeys[UP] = True
                if event.key == K_s:
                    self.pressedKeys[DOWN] = True

            elif event.type == KEYUP:
                if event.key == K_a:
                    self.pressedKeys[LEFT] = False
                if event.key == K_d:
                    self.pressedKeys[RIGHT] = False
                if event.key == K_w:
                    self.pressedKeys[UP] = False
                if event.key == K_s:
                    self.pressedKeys[DOWN] = False

    def pollInput(self):
        if self.pressedKeys[LEFT] and self.pressedKeys[UP]:
            comm.sendMessage("cq")
        elif self.pressedKeys[RIGHT] and self.pressedKeys[UP]:
            comm.sendMessage("ce")
        elif self.pressedKeys[LEFT] and self.pressedKeys[DOWN]:
            comm.sendMessage("cz")
        elif self.pressedKeys[RIGHT] and self.pressedKeys[DOWN]:
            comm.sendMessage("cc")
        elif self.pressedKeys[LEFT] == True:
            comm.sendMessage("ca")
        elif self.pressedKeys[RIGHT] == True:
            comm.sendMessage("cd")
        elif self.pressedKeys[UP] == True:
            comm.sendMessage("cw")
        elif self.pressedKeys[DOWN] == True:
            comm.sendMessage("cs")

        if self.pressedKeys[MOUSE] == True:
            self.sendShotToServer()
            self.pressedKeys[MOUSE] = False

    # calls server for the current game state
    def QueryGameState(self):
        #comm.sendMessage('q')
        p0_info = comm.retrievePlayerState(0)
        p1_info = comm.retrievePlayerState(1)

        # updates position
        self.state.updatePlayerPos(0, p0_info[:2])
        self.state.updatePlayerPos(1, p1_info[:2])

        # update hp and alive status
        self.state.players[0].curr_hp = p0_info[2]
        self.state.players[0].alive = p0_info[3]
        self.state.players[1].curr_hp = p1_info[2]
        self.state.players[1].alive = p1_info[3]

        if self.state.players[0].alive and self.state.players[1].alive:
            self.resetFlag = False

        if self.resetFlag == False: # client is not waiting for server to restart the game
            if self.state.players[0].alive == False:
                self.p1_score += 1
                self.resetFlag = True # makes client wait for the restart
            elif self.state.players[1].alive == False:
                self.p0_score += 1
                self.resetFlag = True

        shots_info = comm.retrieveShotsState()
        self.state.updateShotsPos(shots_info)

    # gets the position the user clicked on the screen and sends to the server
    def sendShotToServer(self):
        msg = "cf-" + str(self.mouse_pos[0]) + ':' + str(self.mouse_pos[1]) + '-'
        comm.sendMessage(msg) # sends info to server
        
    # draws everything on the screen
    def drawScreen(self):
        self.screen.fill(BACKGROUND) # clears the screen

        # draw shots before players, so they don't overlap the sprites
        for shot in self.state.shots:
            if shot.active:
                pygame.draw.circle(self.screen, WHITE, shot.pos, 2) # draws the shot

        #        if self.reloadFlag == False:
        #            self.reloadFlag = True # reset the reload bar
                

        #completeness = int(max(min((self.curr_tick - self.startTick) / float(RELOAD_TIME) * 50, 50), 0))
        #reload_bar = pygame.Rect(0, 0, completeness, 3)
        #reload_bar.center = (self.state.players[self.player_id].pos[0],
        #                     self.state.players[self.player_id].pos[1] - 40)
        #pygame.draw.rect(self.screen, RELOAD_BAR, reload_bar)

        # draw both players
        p0_draw_pos = (self.state.players[0].pos[0] - self.drawPlayerOffset,
                       self.state.players[0].pos[1] - self.drawPlayerOffset)
        p1_draw_pos = (self.state.players[1].pos[0] - self.drawPlayerOffset,
                       self.state.players[1].pos[1] - self.drawPlayerOffset)
        self.screen.blit(self.player0_img, p0_draw_pos)
        self.screen.blit(self.player1_img, p1_draw_pos)

        # draw health bars
        # player 0
        wide = int(max(min(self.state.players[0].curr_hp / float(3) * 50, 50), 0))
        health_bar = pygame.Rect(0, 0, wide, 6)
        health_bar.center = (self.state.players[0].pos[0],
                             self.state.players[0].pos[1] + 40)
        pygame.draw.rect(self.screen, (0, 255, 0), health_bar)

        # player 1
        wide = int(max(min(self.state.players[1].curr_hp / float(3) * 50, 50), 0))
        health_bar = pygame.Rect(0, 0, wide, 6)
        health_bar.center = (self.state.players[1].pos[0],
                             self.state.players[1].pos[1] + 40)
        pygame.draw.rect(self.screen, (0, 255, 0), health_bar)

        #drawing cannons
        # player0 cannon
        if self.player_id == 0:
            playerToMouse_vector = (self.mouse_pos[0] - self.state.players[0].pos[0],
                                    self.mouse_pos[1] - self.state.players[0].pos[1])
            try:
                magnitude = float(math.sqrt(playerToMouse_vector[0] ** 2 + playerToMouse_vector[1] ** 2))
                unity_vector = (playerToMouse_vector[0] / magnitude, playerToMouse_vector[1] / magnitude)

                endpos = (self.state.players[0].pos[0] + unity_vector[0] * 30, 
                              self.state.players[0].pos[1] + unity_vector[1] * 30)
                pygame.draw.line(self.screen, CANNON_1, self.state.players[0].pos, endpos, 10) 
            except:
                pass

        # player1 cannon
        elif self.player_id == 1:
            playerToMouse_vector = (self.mouse_pos[0] - self.state.players[1].pos[0],
                                    self.mouse_pos[1] - self.state.players[1].pos[1])
            try:
                magnitude = float(math.sqrt(playerToMouse_vector[0] ** 2 + playerToMouse_vector[1] ** 2))
                unity_vector = (playerToMouse_vector[0] / magnitude, playerToMouse_vector[1] / magnitude)

                endpos = (self.state.players[1].pos[0] + unity_vector[0] * 30, 
                              self.state.players[1].pos[1] + unity_vector[1] * 30)
                pygame.draw.line(self.screen, CANNON_2, self.state.players[1].pos, endpos, 10) 
            except:
                pass

        self.drawText()

        pygame.display.flip()

    # draws scoreboard and information
    def drawText(self):
        p1_score_text = self.font.render("Player 1: " + str(self.p0_score), True, (255, 255, 255))
        p2_score_text = self.font.render("Player 2: " + str(self.p1_score), True, (255, 255, 255))

        self.screen.blit(p1_score_text, (30, 30))
        self.screen.blit(p2_score_text, (self.size[0]-140, 30))

    def run(self):
        while True:
            # used to draw time dependent stuff
            self.curr_tick = pygame.time.get_ticks()

            self.QueryGameState()
            self.checkForKeyPress()
            self.pollInput()
            self.drawScreen()

            sleep(0.017) # 17ms ~ 60 Hz

if __name__ == '__main__':
    if len(argv) < 3:
        print "Usage:", argv[0], "<port number> <player_id>"
        exit(1)

    port = argv[1]
    player_id = int(argv[2])

    game = ShooterGame(port, player_id)
    game.run()
