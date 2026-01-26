#pragma once

#include <string>
#include <vector>
#include <variant>
#include <cstdint>

enum class ArgumentType : uint8_t
{
	String = 0,
	Integer = 1,
	Float = 2,
	Bool = 3,
};

struct Argument
{
	ArgumentType type;
	std::string stringValue;
	int intValue = 0;
	float floatValue = 0.0f;
	bool boolValue = false;

	Argument() = default;
	Argument(const std::string& value) : type(ArgumentType::String), stringValue(value) {}
	Argument(int value) : type(ArgumentType::Integer), intValue(value) {}
	Argument(float value) : type(ArgumentType::Float), floatValue(value) {}
	Argument(bool value) : type(ArgumentType::Bool), boolValue(value) {}
};

enum class PacketType : uint8_t
{
	RequestJoin,
	HandshakeChallenge,
	HandshakeFinalize,
	JoinResponse,
	ServerConfig,

	RequestFiles,
	FileData,

	DownloadStarted,
	DownloadProgress,

	EmitEvent,
	ClientEmitEvent
};

struct RequestJoinPacket
{
	int playerid;
};

struct HandshakeChallengePacket
{
	std::vector<uint8_t> cookie;
	std::vector<uint8_t> server_public_key;
};

struct HandshakeFinalizePacket
{
	std::vector<uint8_t> cookie;
	std::vector<uint8_t> client_public_key;
};

struct JoinResponsePacket
{
	bool accepted = false;
	uint32_t kcp_conv_id;
	std::string manifest_json;
};

struct ServerConfigPacket
{
	std::vector<uint8_t> master_resource_key;
};

struct RequestFilesPacket
{
	std::vector<std::pair<std::string, std::string>> files;
};

struct FileDataPacket
{
	std::string resourceName;
	std::string relativePath;
	std::string fileHash;
	uint32_t chunkIndex;
	uint32_t totalChunks;
	std::vector<uint8_t> data;
};

struct DownloadStartedPacket 
{
    std::vector<std::pair<std::string, size_t>> files_to_download;
};

struct DownloadProgressPacket 
{
    uint32_t file_index;
    uint64_t bytes_received;
};

struct EmitEventPacket 
{
    int browserId;
    std::string name;
    std::vector<Argument> args;
};

struct ClientEmitEventPacket {
    int browserId;
    std::string name;
    std::vector<Argument> args;
};

using PacketPayload = std::variant<
	RequestJoinPacket,
	HandshakeChallengePacket,
	HandshakeFinalizePacket,
	JoinResponsePacket,
	ServerConfigPacket,

	RequestFilesPacket,
	FileDataPacket,

	DownloadStartedPacket,
	DownloadProgressPacket,

	EmitEventPacket,
	ClientEmitEventPacket
>;

struct NetworkPacket
{
	PacketType type;
	PacketPayload payload;
};