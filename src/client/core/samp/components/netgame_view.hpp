#pragma once

#include "samp/pools/game_pool.hpp"

class INetGameView 
{
public:
	virtual ~INetGameView() = default;

	virtual std::string GetIp() const = 0;
	virtual int GetPort() const = 0;

	virtual int GetLocalPlayerId() const = 0;
	virtual std::string GetLocalPlayerName() const = 0;

	virtual IObjectPool* GetObjectPool() = 0;
    virtual IVehiclePool* GetVehiclePool() = 0;

	virtual CEntity* GetEntityFromObjectId(int objectId) = 0;
};