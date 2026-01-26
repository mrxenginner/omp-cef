#pragma once

class HudManager
{
public:
    HudManager() = default;
    ~HudManager() = default;

    HudManager(const HudManager&) = delete;
    HudManager& operator=(const HudManager&) = delete;

    void Initialize();

private:

};