#ifndef __NAUTY_WRAPPER_HPP__
#define __NAUTY_WRAPPER_HPP__

/// \file nauty.hpp
/// \author Robin Petit
/// \brief Wrapper for nauty's geng and gentreeg in C++

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

#ifdef __linux__
#include <pthread.h>
#endif

/* C's _Thread_local is called thread_local in C++ */
#define _Thread_local thread_local
#include <nauty/gtools.h>
#include <nauty/planarity.h>

#include <nautypp/algorithms.hpp>
#include <nautypp/aliases.hpp>
#include <nautypp/cliquer.hpp>
#include <nautypp/graph.hpp>
#include <nautypp/iterators.hpp>
#include <nautypp/properties.hpp>

namespace nautypp {
namespace version {

static constexpr unsigned major = 0;
static constexpr unsigned minor = 2;

static inline const char* get_version() {
    static char buffer[32];
    sprintf(buffer, "%u.%u", major, minor);
    return buffer;
}

}
}

#ifdef NAUTYPP_SGO
# ifndef NAUTYPP_SMALL_GRAPH_SIZE
// graphs with less than that many vertices do not need to be dynamically
// allocated (similar to SSO)
#  define NAUTYPP_SMALL_GRAPH_SIZE 0
# endif
#endif

extern int _geng_main(int, char**);
extern int _gentreeg_main(int, char**);

static inline void rename_thread(std::thread& thread, const char* name) {
#ifdef __linux__
    pthread_setname_np(
        thread.native_handle(),
        name
    );
#else
    (void)(thread);
    (void)(name);
#endif
}

/// \namespace nautypp
/// \brief Namespace containing everything related to the wrapper
namespace nautypp {
/// \struct NautyParameters
/// \brief Wrapper for the parameters given to geng/gentreeg
struct NautyParameters {
    bool tree          = false;  ///< only generate trees (using gentreeg)
    bool connected     = true;   ///< only generate connected graphs (-c)
    bool biconnected   = false;  ///< only generate biconnected (2-connected) graphs (-C)
    bool triangle_free = false;  ///< only generate triangle-free graphs (-t)
    bool C4_free       = false;  ///< only generate \f$C_4\f$-free graphs (-f)
    bool C5_free       = false;  ///< only generate \f$C_5\f$-free graphs (-p)
    bool K4_free       = false;  ///< only generate \f$K_4\f$-free graphs (-k)
    bool chordal       = false;  ///< only generate chordal graphs (-T)
    bool split         = false;  ///< only generate split graphs (-S)
    bool perfect       = false;  ///< only generate perfect graphs (-P)
    bool claw_free     = false;  ///< only generate claw-free graphs (-F)
    bool bipartite     = false;  ///< only generate bipartite graphs (-b)

    int  V             = -1;  ///< minimum number of vertices
    int  Vmax          = -1;  ///< maximum number of vertices
    int  min_deg       = -1;  ///< minimum degree of vertices
    int  max_deg       = std::numeric_limits<int>::max();  ///< maximum degree of vertices
};


/* ******************** Types ******************** */

/* ******************** Multithreading Tools ******************** */

/// \brief Status of the internal graph buffer.
enum class NautyStatus {
    DATA_AVAILABLE,  ///< Buffer is still active.
    END_OF_THREAD    ///< Buffer is deactivated.
};

// forward declarations
class Nauty;
template <GraphFunctionType Callback>
class NautyWorker;

/*      *************** Containers ***************      */

/// \brief Communication buffer between producer and consumer threads.
///
/// The buffer is separated into a *read* buffer and a *write* buffer.
/// The consumer thread pops from the former, while the producer thread inserts in the latter.
/// When the read buffer is empty, the consumer will swap the two buffers.
template <typename T>
class ContainerBuffer {
public:
    ContainerBuffer() = delete;
    ContainerBuffer(size_t maxsize):
            _size{maxsize},
            _read_buffer(), _write_buffer(),
            _writable{true}, _should_swap{false} {
        _read_buffer.reserve(maxsize);
        _write_buffer.reserve(maxsize);
    }

    ContainerBuffer(const ContainerBuffer&) = delete;
    ContainerBuffer(ContainerBuffer&&) noexcept = default;

    ~ContainerBuffer() {
        if(_read_buffer.size() > 0)
            std::cerr << "Destroying a buffer w/ reading capability";
        if(_write_buffer.size() > 0)
            std::cerr << "Missed a swap (" << _write_buffer.size() << ")\n";
    }

    /// \brief Tries to move something into the write buffer.
    ///
    /// The insertion can only succeed if the buffer is not full.
    /// **Be careful**: if the insertion succeeds, the object is *moved* and not *copied*!
    /// \param G The element to insert.
    /// \return true if the element was inserted.
    inline bool push(T& G) {
        if(_should_swap or write_size() == _size) {
            _should_swap = true;
            return false;
        }
        _write_buffer.push_back(std::move(G));
        return true;
    }

    class EmptyBuffer : public std::exception {
    };

    /// \brief Extract an element from the read buffer.
    ///
    /// The buffers can be swapped if the read buffer is empty
    /// and the write buffer is full.
    /// \throws EmptyBuffer if both the read and write buffers are
    /// empty, and the buffer is deactivated.
    /// \return An element from the read buffer.
    inline T pop() {
        do {
            __verify_not_empty_read();
        } while(_read_buffer.empty() and _writable);
        if(_read_buffer.empty()) {
            if(write_size() > 0)
                std::cerr << "SHOULD NOT THROW\n";
            throw EmptyBuffer();
        }
        T ret{std::move(_read_buffer.back())};
        _read_buffer.pop_back();
        return ret;
    }

    /// \brief Determine whether or not the buffer is active.
    ///
    /// \return true if the buffer is active and false otherwise.
    inline bool writable() const {
        return _writable;
    }

    /// \brief Enable the buffer.
    inline void enable_write() {
        _writable = true;
    }

    // \brief Disable the buffer
    inline void disable_write() {
        _should_swap = true;
        _writable = false;
    }
private:
    size_t                    _size;
    std::vector<T>            _read_buffer;
    std::vector<T>            _write_buffer;
    volatile std::atomic_bool _writable;
    volatile std::atomic_bool _should_swap;

    inline void __verify_not_empty_read() {
        using namespace std::chrono_literals;
        if(read_size() > 0) {
            return;
        } else {
            while(_writable and not _should_swap)
                std::this_thread::sleep_for(10ms);
            if(_should_swap)
                std::swap(_read_buffer, _write_buffer);
            _should_swap = false;
        }
    }

    friend class BaseNautyWorker;
    template <GraphFunctionType Callback>
    friend class NautyWorker;

    /* NO LOCK IS PERFORMED HERE! */
    inline size_t read_size() const {
        return _read_buffer.size();
    }

    /* NO LOCK IS PERFORMED HERE! */
    inline size_t write_size() const {
        return _write_buffer.size();
    }
};

/// Alias for a producer-consumer buffer containing graphs.
/// See Graph.
typedef ContainerBuffer<Graph> NautyContainerBuffer;


/// \brief Container of multiple inter-thread buffers.
///
/// See NautyContainerBuffer.
class NautyContainer {
public:
    NautyContainer(): _worker_buffers() {
    }

    inline std::shared_ptr<NautyContainerBuffer> add_new_buffer(size_t buffer_size) {
        _worker_buffers.push_back(
            std::make_shared<NautyContainerBuffer>(buffer_size)
        );
        return _worker_buffers.back();
    }

    template <typename... Args>
    inline void emplace(Args&&... args) {
        Graph G(std::forward<Args>(args)...);
        while(true) {
            for(auto& buffer : _worker_buffers) {
                if(buffer->writable() and buffer->push(G))
                    return;
            }
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1ms);
        }
    }

    inline void _add_gentree_tree(int* parents, size_t n) {
        emplace(parents, n);
    }

    friend class Nauty;
private:
    typedef std::vector<std::shared_ptr<NautyContainerBuffer>> ContainerVector;
    ContainerVector _worker_buffers;

    inline void set_over() {
        for(auto& buffer : _worker_buffers)
            buffer->disable_write();
    }
};

/*      *************** Workers ***************      */

class BaseNautyWorker {
public:
    BaseNautyWorker(std::shared_ptr<NautyContainerBuffer> buffer):
            _buffer{std::move(buffer)} {
    }

    ~BaseNautyWorker() = default;

    void run() {
        while(true) {
            try {
                Graph G{_buffer->pop()};
                (*this)(G);
            } catch(NautyContainerBuffer::EmptyBuffer&) {
                break;
            }
        }
    }

    virtual void operator()(Graph& G) = 0;
protected:
    typedef std::shared_ptr<NautyContainerBuffer> BufferPtr;
    BufferPtr  _buffer;
};

template <GraphFunctionType GraphFunction>
class NautyWorker final : public BaseNautyWorker {
public:
    typedef GraphFunction callback_t;

    NautyWorker(std::shared_ptr<NautyContainerBuffer> buffer, callback_t callback):
            BaseNautyWorker(buffer), _callback{callback} {
    }

    NautyWorker(NautyWorker&) = delete;
    NautyWorker(NautyWorker&&) = default;

#ifdef NAUTYPP_DEBUG
    ~NautyWorker() noexcept(false) {
        if(_buffer->writable())
            throw std::runtime_error("Destroying a worker with a readable buffer.");
        if(_buffer->read_size() > 0)
            std::cerr << "Destroying worker with read buffer size "
                      << _buffer->read_size() << std::endl;
    }
#else
    ~NautyWorker() = default;
#endif

    virtual void operator()(Graph& G) override final {
        _callback(G);
    }
protected:
    callback_t _callback;
};

template <GraphRefFunctionType Callback>
class NautyWorkerWrapper final : public BaseNautyWorker {
public:
    NautyWorkerWrapper(std::shared_ptr<NautyContainerBuffer> buffer):
            BaseNautyWorker(buffer), _callback() {
    }

    virtual void operator()(Graph& G) override final {
        _callback(G);
    }

    inline void join(NautyWorkerWrapper& other) {
        _callback.join(std::move(other._callback));
    }

    inline const typename Callback::ResultType& get() const& {
        return _callback.get();
    }

    inline typename Callback::ResultType&& get() && {
        return _callback.get();
    }
private:
    Callback _callback;
};

/*      *************** Nauty ***************      */

/// \brief Wrapper for geng/gentreeg
class Nauty {
public:
    Nauty() = default;

    /// \brief Run some callback on all graphs found in a file.
    ///
    /// The file must contain graphs either in the format graph6 or sparse6.
    /// The file is not closed after reading.
    /// Unless you explicitely need to hand the file on your own,
    /// prefer the version requiring the path to the file.
    ///
    /// \param callback The function to execute on every graph.
    /// \param f The the file containing the graphs.
    /// \param max_graph_size The maximal order of a graph in the file.
    /// \param nb_workers The number of threads to create to dispatch the generated graphs.
    /// \param worker_buffer_size The buffer size for every worker.
    ///
    /// **Example**:
    /// \include multithreaded/graph_reader.cpp
    template <GraphFunctionType GraphFunction>
    void run_async(GraphFunction callback,
            FILE* f,
            size_t max_graph_size=100,
            size_t nb_workers=std::thread::hardware_concurrency(),
            size_t worker_buffer_size=5'000) {
        auto [worker_threads, workers] = make_workers(
            callback, nb_workers, worker_buffer_size
        );
        auto reader_thread{start_reader_thread(f, max_graph_size)};
        reader_thread.join();
        for(size_t i{0}; i < nb_workers; ++i)
            worker_threads.at(i).join();
    }

    /// \brief Run some callback on all graphs found in a file.
    ///
    /// The file must contain graphs either in the format graph6 or sparse6.
    ///
    /// \param callback The function to execute on every graph.
    /// \param file_path The path to the file containing the graphs.
    /// \param max_graph_size The maximal order of a graph in the file.
    /// \param nb_workers The number of threads to create to dispatch the generated graphs.
    /// \param worker_buffer_size The buffer size for every worker.
    ///
    /// **Example**:
    /// \include multithreaded/graph_reader.cpp
    template <GraphFunctionType GraphFunction>
    void run_async(GraphFunction callback,
            const std::string& file_path,
            size_t max_graph_size=100,
            size_t nb_workers=std::thread::hardware_concurrency(),
            size_t worker_buffer_size=5'000) {
        auto [worker_threads, workers] = make_workers(
            callback, nb_workers, worker_buffer_size
        );
        auto reader_thread{start_reader_thread(file_path, max_graph_size)};
        reader_thread.join();
        for(size_t i{0}; i < nb_workers; ++i)
            worker_threads.at(i).join();
    }

    /// \brief Run some callback on all graphs generated by geng/gentreeg.
    ///
    /// \param callback The function to execute on every graph.
    /// \param nb_workers The number of threads to create to dispatch the generated graphs.
    /// \param worker_buffer_size The buffer size for every worker.
    ///
    /// **Example**:
    /// \include multithreaded/count_triangle_free_graphs.cpp
    template <GraphFunctionType GraphFunction>
    void run_async(GraphFunction callback,
            const NautyParameters& parameters,
            size_t nb_workers=std::thread::hardware_concurrency(),
            size_t worker_buffer_size=5'000) {
        auto [worker_threads, workers] = make_workers(
            callback, nb_workers, worker_buffer_size
        );
        auto geng_thread{start_nauty(parameters)};
        geng_thread.join();
        for(size_t i{0}; i < nb_workers; ++i)
            worker_threads.at(i).join();
    }

    /// Alternative version of run_async.
    ///
    /// Here, the callback is not provided by reference but by type.
    ///
    /// The template type \a Callback must:
    /// - provide a type `Callback::ResultType`
    /// - define `operator()(const Graph&)`
    /// - define `join(Callback&& other)`
    /// - define `ResultType&& get()`
    ///
    /// **Example**:
    /// \include multithreaded/heavy_callback.cpp
    template <GraphRefFunctionType Callback>
    auto run_async(
            const NautyParameters& parameters,
            size_t nb_workers=std::thread::hardware_concurrency(),
            size_t worker_buffer_size=5'000) -> typename Callback::ResultType {
        Nauty::reset_container();
        std::vector<NautyWorkerWrapper<Callback>> wrappers;
        std::vector<std::thread> workers;
        wrappers.reserve(nb_workers);
        workers.reserve(nb_workers);
        static char name_buffer[32];
        for(size_t i{0}; i < nb_workers; ++i) {
            auto worker_buffer{Nauty::_container->add_new_buffer(worker_buffer_size)};
            wrappers.emplace_back(worker_buffer);
            workers.emplace_back(
                &NautyWorkerWrapper<Callback>::run,
                std::addressof(wrappers.back())
            );
            std::sprintf(name_buffer, "Worker %u", static_cast<unsigned>(i+1));
            rename_thread(workers.back(), name_buffer);
        }
        auto geng_thread{start_nauty(parameters)};
        geng_thread.join();
        for(size_t i{0}; i < nb_workers; ++i)
            workers.at(i).join();
        for(size_t idx{1}; idx < nb_workers; ++idx)
            wrappers.at(0).join(wrappers.at(idx));
        return static_cast<NautyWorkerWrapper<Callback>&&>(wrappers.at(0)).get();
    }

    static inline std::unique_ptr<NautyContainer>& get_container() {
#ifdef NAUTYPP_DEBUG
        if(not Nauty::_container)
            throw std::runtime_error("No initialized container");
#endif
        return Nauty::_container;
    }

private:
    //NautyParameters parameters;

    inline const char* get_nauty_name(const NautyParameters& parameters) const {
        if(parameters.tree)
            return "nauty-gentreeg";
        else
            return "nauty-geng";
    }

    inline std::thread start_reader_thread(
            const std::string& file_path,
            size_t max_graph_size) const {
        std::thread ret(
            [this, max_graph_size](const std::string& path) {
                int  n{static_cast<int>(max_graph_size)};
                int  m{SETWORDSNEEDED(n)};
                auto G{static_cast<graph*>(ALLOCS(m*n, sizeof(graph)))};
                FILE* f{fopen(path.c_str(), "r")};
                if(f == NULL)  // TODO handle error properly
                    return;
                boolean directed;
                while(readgg(f, G, 0, &m, &n, &directed) != nullptr) {
                    Nauty::get_container()->emplace(G, n, true, (bool)directed);
                }
                this->get_container()->set_over();
                fclose(f);
                FREES(G);
            },
            file_path
        );
        return ret;
    }

    inline std::thread start_reader_thread(
            FILE* f,
            size_t max_graph_size) const {
        std::thread ret(
            [this, f, max_graph_size]() {
                int  n{static_cast<int>(max_graph_size)};
                int  m{SETWORDSNEEDED(n)};
                auto G{static_cast<graph*>(ALLOCS(m*n, sizeof(graph)))};
                boolean directed;
                while(readgg(f, G, 0, &m, &n, &directed) != nullptr) {
                    Nauty::get_container()->emplace(G, n, true, (bool)directed);
                }
                this->get_container()->set_over();
                FREES(G);
            }
        );
        return ret;
    }

    inline std::thread start_nauty(const NautyParameters& parameters) {
        std::thread ret{
            parameters.tree
            ? start_gentreeg(parameters)
            : start_geng(parameters)
        };
        rename_thread(
            ret,
            get_nauty_name(parameters)
        );
        return ret;
    }

    inline std::thread start_gentreeg(const NautyParameters& parameters) {
        strcpy(params[0], "gentreeg");
        std::sprintf(
            params[1],
            "-D%d%s",
            parameters.max_deg,
#ifdef NAUTYPP_DEBUG
            ""
#else
            "q"
#endif
        );
        std::sprintf(
            params[2], "%d", parameters.V
        );
        for(auto i{0}; i < Nauty::GENG_ARGC; ++i)
            geng_argv[i] = params[i];
        std::thread t(
            [this]() {
                _gentreeg_main(Nauty::GENG_ARGC, geng_argv);
                this->set_container_over();
            }
        );
        return t;
    }

    inline std::thread start_geng(const NautyParameters& parameters) {
        strcpy(params[0], "geng");
        for(auto i{0}; i < Nauty::GENG_ARGC; ++i)
            geng_argv[i] = params[i];
        std::thread t(
            [this, &parameters]() {
                int min_deg, max_deg;
                for(int V{parameters.V}; V <= parameters.Vmax; ++V) {
                    min_deg = std::min(parameters.min_deg, V-1);
                    max_deg = std::min(parameters.max_deg, V-1);
                    std::sprintf(
                        params[1],
                        "-%s%s%s%s%s%s%s%s%s%s%sd%dD%d%s",
                        parameters.connected     ? "c" : "",
                        parameters.biconnected   ? "C" : "",
                        parameters.triangle_free ? "t" : "",
                        parameters.C4_free       ? "f" : "",
                        parameters.C5_free       ? "p" : "",
                        parameters.K4_free       ? "k" : "",
                        parameters.chordal       ? "T" : "",
                        parameters.split         ? "S" : "",
                        parameters.perfect       ? "P" : "",
                        parameters.claw_free     ? "F" : "",
                        parameters.bipartite     ? "b" : "",
                        min_deg,
                        max_deg,
#ifdef NAUTYPP_DEBUG
                        ""
#else
                        "q"
#endif
                    );
                    std::sprintf(this->geng_argv[2], "%d", V);
                    _geng_main(Nauty::GENG_ARGC, this->geng_argv);
                }
                this->set_container_over();
            }
        );
        return t;
    }

    static inline constexpr auto GENG_ARGV_BUFFER_SIZE{64};
    static inline constexpr auto GENG_ARGC{3};
    char params[Nauty::GENG_ARGC][Nauty::GENG_ARGV_BUFFER_SIZE];
    char* geng_argv[Nauty::GENG_ARGC];

    static void reset_container() {
        Nauty::_container.reset(new NautyContainer());
    }

    static std::unique_ptr<NautyContainer> _container;
    inline void set_container_over() {
        Nauty::get_container()->set_over();
    }

    template <GraphFunctionType GraphFunction>
    auto make_workers(GraphFunction callback,
            size_t nb_workers, size_t worker_buffer_size) {
        Nauty::reset_container();
        std::vector<std::thread> worker_threads;
        std::vector<NautyWorker<GraphFunction>> workers;
        worker_threads.reserve(nb_workers);
        workers.reserve(nb_workers);
        static char name_buffer[32];
        for(size_t i{0}; i < nb_workers; ++i) {
            auto worker_buffer{
                Nauty::_container->add_new_buffer(worker_buffer_size)
            };
            workers.emplace_back(worker_buffer, callback);
            worker_threads.emplace_back(
                &NautyWorker<GraphFunction>::run,
                &workers.back()
            );
            std::sprintf(name_buffer, "Worker %u", static_cast<unsigned>(i+1));
            rename_thread(worker_threads.back(), name_buffer);
        }
        return std::make_pair(
            std::move(worker_threads),
            std::move(workers)
        );
    }
};

}  // namespace nautypp

#include <nautypp/impl.hpp>
#endif
