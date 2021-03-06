#include "player.h"
#include <glm/gtx/intersect.hpp>
#include "game.h"
#include "gameObject.h"
#include "shader.h"
#include "resource_manager.h"
#include "VoxelLoader.h"
#include "VoxelModel.h"
#include "level.h"
#include "camera.h"

std::vector<glm::vec3> bulletColors = {
	glm::vec3(1.0f, 0.2f, 0.6f),
	glm::vec3(0.4f, 0.8f, 1.0f),
	glm::vec3(1.0f, 1.0f, 0.4f)
};

void Player::loadModels()
{
	VoxelLoader::loadModel("models/player/0.vox", "player_0");
	VoxelLoader::loadModel("models/player/1.vox", "player_1");

	VoxelLoader::loadModel("models/player/death0.vox", "player_death0");
	VoxelLoader::loadModel("models/player/death1.vox", "player_death1");
	VoxelLoader::loadModel("models/player/death2.vox", "player_death2");
	VoxelLoader::loadModel("models/player/death3.vox", "player_death3");
	VoxelLoader::loadModel("models/player/death4.vox", "player_death4");
}

Player::Player(Game &game, VoxelRenderer &renderer) : Character(game, renderer)
{
	speed = 10.0f;
	bulletSpeed = 23.0f;
	bulletScale = 0.5f;
	fireCooldown = 0.6f;
	modelUpdateDelay = 0.5f;

	//models must be loaded first!
	charModels.push_back(&VoxelLoader::getModel("player_0"));
	charModels.push_back(&VoxelLoader::getModel("player_1"));

	deathModels.push_back(&VoxelLoader::getModel("player_death0"));
	deathModels.push_back(&VoxelLoader::getModel("player_death1"));
	deathModels.push_back(&VoxelLoader::getModel("player_death2"));
	deathModels.push_back(&VoxelLoader::getModel("player_death3"));
	deathModels.push_back(&VoxelLoader::getModel("player_death4"));

	//load audio
	shootAudio.load("audio/gunshot.wav");
	damageAudio.load("audio/player_damage.wav");
	dashAudio.load("audio/player_dash.wav");
	deathAudio.load("audio/player_death.wav");
	jumpAudio.load("audio/player_jump.wav");
	landAudio.load("audio/player_land.wav");
}

void Player::updateState(float dt)
{
	//update model
	if (game.elapsedTime - lastModelUpdate >= modelUpdateDelay)
	{
		nextModel();
	}

	moveBullets(dt);

	//check collisions
	game.currentLevel->checkPlayerBulletCollision(*this);
	game.currentLevel->checkBulletEnemyCollisions(*this);
	game.currentLevel->checkPlayerPickupCollision(*this);

	//update bullet trails
	for (auto itr = bullets.begin(); itr != bullets.end(); itr++)
	{
		itr->trail.update(dt);
	}

	//check if tint needs to be changed
	if (!poweredUp && game.elapsedTime - lastDamaged >= tintDuration)
		tintColor = glm::vec3(1.0f, 1.0f, 1.0f);

	//check if powerup status needs to change
	if (poweredUp && game.elapsedTime - lastPowerUpTime >= powerUpDuration)
	{
		tintColor = glm::vec3(1.0f, 1.0f, 1.0f);
		poweredUp = false;
		fireCooldown *= 2;
	}
}

void Player::processInput(float dt)
{
	movePlayer(dt);
	moveVertical(dt);
	rotatePlayer(dt);

	//mouse1: fire
	if (game.mouse1 && game.elapsedTime - lastFireTime >= fireCooldown)
	{
		lastFireTime = game.elapsedTime;
		fire();
	}

	//mouse2: dash
	if (game.mouse2 && game.elapsedTime - lastDashTime >= dashCooldown)
	{
		lastDashTime = game.elapsedTime;
		game.audioEngine.play(dashAudio);

		//get direction player is facing, and set the dash direction
		glm::mat4 modelMat = glm::mat4(1.0f);
		modelMat = glm::rotate(modelMat, glm::radians(rotate.y), glm::vec3(0.0f, 1.0f, 0.0f));
		dashDirection = glm::normalize(modelMat * glm::vec4(-1.0f, 0.0f, 0.0f, 1.0f));
	}
}

void Player::takeDamage()
{
	if (poweredUp)
		return;
	if (game.elapsedTime - lastDamaged <= tintDuration)
		return;
	if (state != ALIVE)
		return;
	hp--;
	game.audioEngine.play(damageAudio);
	lastDamaged = game.elapsedTime;
	tintColor = glm::vec3(1.0f, 0.0f, 0.0f);
	if (hp == 0)
	{
		game.audioEngine.play(deathAudio);
		state = DYING;
		modelUpdateDelay = 0.2f;
		modelIndex = 0;
	}
}

//powerup: makes player invincible and increases fire rate
void Player::powerUp()
{
	poweredUp = true;
	tintColor = glm::vec3(1.0f, 0.8f, 0.0f);
	fireCooldown /= 2;
	lastPowerUpTime = game.elapsedTime;
}

void Player::movePlayer(float dt)
{
	glm::vec3 fb = game.mainCamera->Position - this->pos; //vector from player to camera (forward/back movement)
	fb = glm::normalize(glm::vec3(fb.x, 0, fb.z));

	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 lr = glm::normalize(glm::cross(up, fb)); //vector pointing to player's right (left/right movement)
	glm::vec3 movement = glm::vec3(0.0f, 0.0f, 0.0f);

	//get movement from keyboard input
	if (game.Keys[GLFW_KEY_W])
	{
		movement -= speed * dt * fb;
	}
	if (game.Keys[GLFW_KEY_A])
	{
		movement -= speed * dt * lr;
	}
	if (game.Keys[GLFW_KEY_S])
	{
		movement += speed * dt * fb;
	}
	if (game.Keys[GLFW_KEY_D])
	{
		movement += speed * dt * lr;
	}

	//now move the player along x and z axis and check for collisions
	glm::vec3 displacement = glm::vec3(0.0f, 0.0f, 0.0f);

	pos.z += movement.z;
	displacement = game.currentLevel->checkPlayerLevelCollision(*this);
	pos.z += displacement.z;

	pos.x += movement.x;
	displacement = game.currentLevel->checkPlayerLevelCollision(*this);
	pos.x += displacement.x;

	//if player is dashing, move along dash direction and diminish the dash speed
	if (dashDirection != glm::vec3(0, 0, 0))
	{
		pos += dashVelocity * dashDirection * dt;
		dashDirection.x = (dashDirection.x - (2 * dt) < 0) ? 0 : dashDirection.x - (2 * dt);
		dashDirection.y = (dashDirection.y - (2 * dt) < 0) ? 0 : dashDirection.y - (2 * dt);
		dashDirection.z = (dashDirection.z - (2 * dt) < 0) ? 0 : dashDirection.z - (2 * dt);
	}

	//move player along with level if he is grounded
	if (grounded)
	{
		pos.z -= game.currentLevel->islandSpeed * dt;
	}

	if (game.currentLevel->outOfBounds(*this))
	{ //if player is out of bounds
		state = DEAD;
	}
}

void Player::moveVertical(float dt)
{
	if (grounded)
	{
		if (game.Keys[GLFW_KEY_SPACE])
		{ //press space to jump
		    game.audioEngine.play(jumpAudio);
			grounded = false;
			verticalVelocity = 1.0f;
		}
	}

	if (grounded)
	{ //move player down and get displacement to test if there is ground below him
		pos.y += speed * dt * verticalVelocity;
		glm::vec3 displacement = game.currentLevel->checkPlayerLevelCollision(*this);
		pos.y += displacement.y;
		if (displacement.y <= 0)
			grounded = false; //if there is no ground below him, set grounded to false
	}

	if (!grounded)
	{
		pos.y += speed * dt * verticalVelocity; //move player vertically and then test for a collision
		verticalVelocity -= dt;
		glm::vec3 displacement = game.currentLevel->checkPlayerLevelCollision(*this);
		pos.y += displacement.y;

		if (displacement.y > 0)
		{ //if the displacement from collision resolution is positive, then the player has hit the ground
		    game.audioEngine.play(landAudio);
			grounded = true;
			verticalVelocity = -0.1f;
		}
		if (displacement.y < 0)
			verticalVelocity = 0.0f; //if displacement is negative, player has hit a ceiling, so set verticalVelocity=0
	}
}

void Player::rotatePlayer(float dt)
{ //rotate player based on mouse position
	glm::mat4 modelMat = glm::mat4(1.0f);
	glm::mat4 projection = game.mainCamera->GetProjectionMatrix();
	glm::mat4 view = game.mainCamera->GetViewMatrix();
	VoxelModel &model = *charModels[modelIndex];

	//obtain midPos, the point at the middle of the player model
	modelMat = glm::translate(modelMat, pos);
	glm::vec3 midPos = glm::vec3(0.5f * scale * model.getSize().x, 0.5f * scale * model.getSize().y, 0.5f * scale * model.getSize().z);
	midPos = modelMat * glm::vec4(midPos, 1.0f);

	//get intersection of line pointing from the cursor with the plane: y = midPos.y
	glm::vec3 cursorPosNear = glm::unProject(glm::vec3(game.mouseX, game.Height - game.mouseY, 0.0f), view, projection, glm::vec4(0, 0, game.Width, game.Height));
	glm::vec3 cursorPosFar = glm::unProject(glm::vec3(game.mouseX, game.Height - game.mouseY, 1.0f), view, projection, glm::vec4(0, 0, game.Width, game.Height));
	float distance;
	glm::intersectRayPlane(cursorPosNear, glm::normalize(cursorPosFar - cursorPosNear), midPos, glm::vec3(0, 1, 0), distance);
	glm::vec3 cursorPos = cursorPosNear + distance * glm::normalize(cursorPosFar - cursorPosNear); //intersection point
	glm::vec3 playerToCursor = glm::normalize(cursorPos - midPos);

	//get angle between playerToCursor and the vector <-1, 0, 0> (direction player is facing when he first spawns)
	float angle = acos(glm::dot(playerToCursor, glm::vec3(-1.0f, 0.0f, 0.0f)));

	//use cross product to check if angle should be negated
	glm::vec3 cross = glm::normalize(glm::cross(playerToCursor, glm::vec3(-1.0f, 0.0f, 0.0f)));
	if (glm::abs(cross.y - 1.0f) <= .01f)
	{
		angle = -angle;
	}

	//finally, rotate the player
	rotate.y = glm::degrees(angle);
}

void Player::fire()
{
	VoxelModel *model;
	if (state == ALIVE)
		model = charModels[modelIndex];
	else
		model = deathModels[modelIndex];

	glm::mat4 modelMat = glm::mat4(1.0f);
	modelMat = glm::rotate(modelMat, glm::radians(rotate.y), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::vec3 direction = glm::normalize(modelMat * glm::vec4(-1.0f, 0.0f, 0.0f, 1.0f)); //direction the player is facing

	modelMat = glm::mat4(1.0f);
	modelMat = glm::translate(modelMat, pos);
	glm::vec3 midPos = glm::vec3(0.5f * scale * model->getSize().x, 0.5f * scale * model->getSize().y, 0.5f * scale * model->getSize().z);
	midPos = modelMat * glm::vec4(midPos, 1.0f); //middle point of the player model

	glm::vec3 bulletColor = bulletColors[rand() % bulletColors.size()];
	bullets.emplace_back(midPos, direction, bulletColor, rotate.y, bulletScale, 10);
	game.audioEngine.play(shootAudio);
}