# MxOEmu (The Matrix Online Emulator)

Welcome to the MxOEmu repository. This repository contains the tools, server, and client hacks required to run and connect to the emulated Matrix Online universe.

## Repository Structure

- **Server**: Contains the \Reality\ server emulator, \Proxy\, \Sniffer\, and all required compilation dependencies.
- **Client**: Contains client-side utilities and modifications, such as \mxohax\ (a detours-based patcher for the original MxO client).

## Recent Updates
- **Content Integration**: The Reality Server has been populated with authentic \mxo-hd\ data dumps (mobs, items, etc.).
- **Engine Upgrade**: Replaced the original single-threaded AI bottleneck with a high-performance Spatial LOD Manager (adapted from CastleEngine), allowing the server to dynamically handle thousands of concurrent AI entities without freezing the main thread.

## Building the Server
The \Reality\ server is built with MSBuild / Visual Studio. Ensure you have the appropriate legacy VS2010 toolchains for the \cryptlib\ dependencies if you are building from scratch.

