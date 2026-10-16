/* abi.h: the stable C ABI of project e18. Four functions, and nothing else crosses
 * the boundary between the Rust device service and the C inference daemon.
 *
 * WHY A C ABI RATHER THAN A SOCKET, stated here because this header is the contract
 * and the reason belongs with it. Two reasons, in this order.
 *
 *   1. Either side can be replaced without the other being rebuilt. The device
 *      service owns the serial ports, the frame decoder and the log, and it changes
 *      whenever the installation changes. The inference daemon owns the model, the
 *      grammar and the sampler, and it changes whenever the engine is upgraded.
 *      Those are different lifetimes and, in a product, different authors. A shared
 *      object behind four declarations lets one be swapped on disk while the other
 *      binary stays byte for byte what was tested.
 *
 *   2. A C ABI is the one boundary both languages speak. Rust reaches C with no
 *      binding generator and no serialisation format; C++ reaches it with
 *      extern "C"; Python reaches it with ctypes. A socket needs a protocol, and a
 *      protocol is a second thing to version. The socket in daemon.c exists as well,
 *      for the case where the two must be separate processes, and it is a thin
 *      server over exactly these four functions rather than a design of its own.
 *
 * What that costs is stated in the chapter and measured by abi_test.c: one more
 * indirection per call against an in-process call to edgellm_decide, and the loss
 * of restart isolation, which the socket form restores.
 *
 * RULES THIS HEADER OBEYS, because an ABI that is not disciplined is not an ABI:
 *
 *   - No structure, no enum and no typedef crosses the boundary. Only const char *,
 *     char *, size_t, int, float and void. A caller therefore has nothing whose
 *     layout can change under it.
 *   - One engine per process. edgellm_abi_decide takes no handle on purpose: a
 *     handle is a pointer to a structure whose definition would have to be shared,
 *     and one model is all a 4 GB board holds anyway. A second edgellm_abi_open
 *     before edgellm_abi_close is refused rather than silently reopened.
 *   - Every call is serialised internally, so a multi threaded caller is safe and a
 *     caller does not have to know that.
 *   - EDGELLM_ABI_VERSION is bumped on ANY change to the four declarations below.
 *     edgellm_abi_version() returns the value the LIBRARY was built with, so a
 *     caller compares it against the value it compiled against and refuses a
 *     mismatch. That comparison is the only defence against a library replaced
 *     under a running service, which is the whole point of reason 1 above.
 */
#ifndef EDGELLM_E18_ABI_H
#define EDGELLM_E18_ABI_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* major * 1000 + minor. A minor bump adds a function and keeps every declaration
 * below unchanged; a major bump is anything else, including one added parameter.
 * 1000 is the first published version, Wednesday 14 October 2026. */
#define EDGELLM_ABI_VERSION 1000
#define EDGELLM_ABI_VERSION_MAJOR (EDGELLM_ABI_VERSION / 1000)

/* The version the LIBRARY was built with. A caller refuses a different major and
 * logs a different minor. Safe to call before edgellm_abi_open. */
unsigned edgellm_abi_version(void);

/* Loads the model, reads the grammar and the plain text system prompt, decodes that
 * prompt once and keeps its state. state_path may be NULL to keep the state
 * resident. temperature 0 means greedy.
 *
 * Returns 0 on success; -1 if an engine is already open; -2 if a path cannot be read
 * or the engine refuses to load. On any failure nothing is open. */
int edgellm_abi_open(const char *model_path, const char *grammar_path,
                     const char *prompt_path, const char *state_path,
                     int n_ctx, int n_threads, int n_predict, float temperature);

/* One decision. user is the plain user text for this event, already carrying the
 * chat markers the model wants (the caller owns the template; see
 * common/c/src/edgellm_main.c for why that lives above this boundary). out receives
 * the grammar constrained completion, NUL terminated.
 *
 * Returns the number of bytes written, not counting the NUL; -1 if no engine is
 * open; -2 if the engine reported an error; -3 if out_len is too small for the
 * answer, in which case out is left as an empty string rather than truncated,
 * because a truncated JSON object parses as nothing and would be logged as a
 * missing literal. */
int edgellm_abi_decide(const char *user, char *out, size_t out_len);

/* Closes the engine. Safe to call when nothing is open. */
void edgellm_abi_close(void);

#ifdef __cplusplus
}
#endif
#endif /* EDGELLM_E18_ABI_H */
