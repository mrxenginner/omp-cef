#include "netgame.hpp"

sampapi::v037r1::CNetGame* NetGameView_R1::GetNetGame() const
{
    return sampapi::v037r1::RefNetGame();
}

IObjectPool* NetGameView_R1::GetObjectPool() 
{
    return &object_pool_wrapper_;
}

IVehiclePool* NetGameView_R1::GetVehiclePool() 
{
    return &vehicle_pool_wrapper_;
}

std::string NetGameView_R1::GetIp() const
{
    if (const auto* netGame = GetNetGame()) {
        return std::string{ netGame->m_szHostAddress };
    }

    return "";
}

int NetGameView_R1::GetPort() const 
{
    if (const auto* netGame = GetNetGame()) {
        return netGame->m_nPort;
    }

    return -1;
}

int NetGameView_R1::GetLocalPlayerId() const
{
    auto* netGame = GetNetGame();
    if (netGame && netGame->GetPlayerPool()) {
        return netGame->GetPlayerPool()->m_localInfo.m_nId;
    }

    return -1;
}

std::string NetGameView_R1::GetLocalPlayerName() const
{
    auto* netGame = GetNetGame();
    if (netGame && netGame->GetPlayerPool()) {
        return netGame->GetPlayerPool()->m_localInfo.m_szName;
    }

    return "";
}

CEntity* NetGameView_R1::GetEntityFromObjectId(int objectId) 
{
    auto pool = GetObjectPool();
    if (!pool)
        return nullptr;

    auto sampObject = pool->Get(objectId);
    if (!sampObject)
        return nullptr;

    return sampObject->GetGameEntity();
}