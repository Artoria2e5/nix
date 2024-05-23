#pragma once
///@file

#include "ssh-store-config.hh"
#include "store-api.hh"
#include "ssh.hh"
#include "callback.hh"
#include "pool.hh"
#include "serve-protocol-impl.hh"

namespace nix {

struct LegacySSHStoreConfig : virtual CommonSSHStoreConfig
{
    using CommonSSHStoreConfig::CommonSSHStoreConfig;

    const Setting<Strings> remoteProgram{this, {"nix-store"}, "remote-program",
        "Path to the `nix-store` executable on the remote machine."};

    const Setting<int> maxConnections{this, 1, "max-connections",
        "Maximum number of concurrent SSH connections."};

    /**
     * Hack for hydra
     */
    Strings extraSshArgs = {};

    const std::string name() override { return "SSH Store"; }

    std::string doc() override;
};

struct LegacySSHStore : public virtual LegacySSHStoreConfig, public virtual Store
{
    // Hack for getting remote build log output.
    // Intentionally not in `LegacySSHStoreConfig` so that it doesn't appear in
    // the documentation
    const Setting<int> logFD{this, -1, "log-fd", "file descriptor to which SSH's stderr is connected"};

    struct Connection;

    std::string host;

    ref<Pool<Connection>> connections;

    SSHMaster master;

    static std::set<std::string> uriSchemes() { return {"ssh"}; }

    LegacySSHStore(
        std::string_view scheme,
        std::string_view host,
        const Params & params);

private:
    LegacySSHStore(
        std::string_view scheme,
        std::string host,
        const Params & params);
public:

    ref<Connection> openConnection();

    std::string getUri() override;

    void queryPathInfoUncached(const StorePath & path,
        Callback<std::shared_ptr<const ValidPathInfo>> callback) noexcept override;

    std::map<StorePath, UnkeyedValidPathInfo> queryPathInfosUncached(
        const StorePathSet & paths);

    void addToStore(const ValidPathInfo & info, Source & source,
        RepairFlag repair, CheckSigsFlag checkSigs) override;

    void narFromPath(const StorePath & path, Sink & sink) override;

    /**
     * Gives Hands over the connection temporarily as source.
     *
     * Caller must be sure to not consume more than the NAR.
     */
    void narFromPath(const StorePath & path, std::function<void(Source &)> fun);

    std::optional<StorePath> queryPathFromHashPart(const std::string & hashPart) override
    { unsupported("queryPathFromHashPart"); }

    StorePath addToStore(
        std::string_view name,
        const SourcePath & path,
        ContentAddressMethod method,
        HashAlgorithm hashAlgo,
        const StorePathSet & references,
        PathFilter & filter,
        RepairFlag repair) override
    { unsupported("addToStore"); }

    virtual StorePath addToStoreFromDump(
        Source & dump,
        std::string_view name,
        FileSerialisationMethod dumpMethod = FileSerialisationMethod::Recursive,
        ContentAddressMethod hashMethod = FileIngestionMethod::Recursive,
        HashAlgorithm hashAlgo = HashAlgorithm::SHA256,
        const StorePathSet & references = StorePathSet(),
        RepairFlag repair = NoRepair) override
    { unsupported("addToStore"); }

public:

    BuildResult buildDerivation(const StorePath & drvPath, const BasicDerivation & drv,
        BuildMode buildMode) override;

    /**
     * Unsafe if connection in pool is greater than 1!
     *
     * Can make safe once we have C++23 `std::move_only_function`.
     */
    std::function<BuildResult()> buildDerivationAsync(
        const StorePath & drvPath, const BasicDerivation & drv,
        const ServeProto::BuildOptions & options);

    void buildPaths(const std::vector<DerivedPath> & drvPaths, BuildMode buildMode, std::shared_ptr<Store> evalStore) override;

    void ensurePath(const StorePath & path) override
    { unsupported("ensurePath"); }

    virtual ref<SourceAccessor> getFSAccessor(bool requireValidPath) override
    { unsupported("getFSAccessor"); }

    /**
     * The default instance would schedule the work on the client side, but
     * for consistency with `buildPaths` and `buildDerivation` it should happen
     * on the remote side.
     *
     * We make this fail for now so we can add implement this properly later
     * without it being a breaking change.
     */
    void repairPath(const StorePath & path) override
    { unsupported("repairPath"); }

    void computeFSClosure(const StorePathSet & paths,
        StorePathSet & out, bool flipDirection = false,
        bool includeOutputs = false, bool includeDerivers = false) override;

    StorePathSet queryValidPaths(const StorePathSet & paths,
        SubstituteFlag maybeSubstitute = NoSubstitute) override;

    /**
     * Custom variation that atomically creates temp locks on the remote
     * side.
     *
     * This exists to prevent a race where the remote host
     * garbage-collects paths that are already there. Optionally, ask
     * the remote host to substitute missing paths.
     */
    StorePathSet queryValidPaths(const StorePathSet & paths,
        bool lock,
        SubstituteFlag maybeSubstitute = NoSubstitute);

    /**
     * Just exists because this is exactly what Hydra was doing, and we
     * don't yet want an algorithmic change.
     */
    void addMultipleToStoreLegacy(Store & srcStore, const StorePathSet & paths);

    void connect() override;

    unsigned int getProtocol() override;

    struct ConnectionStats {
        size_t bytesReceived, bytesSent;
    };

    ConnectionStats getConnectionStats();

    pid_t getConnectionPid();

    /**
     * The legacy ssh protocol doesn't support checking for trusted-user.
     * Try using ssh-ng:// instead if you want to know.
     */
    std::optional<TrustedFlag> isTrustedClient() override
    {
        return std::nullopt;
    }

    void queryRealisationUncached(const DrvOutput &,
        Callback<std::shared_ptr<const Realisation>> callback) noexcept override
    // TODO: Implement
    { unsupported("queryRealisation"); }
};

}
