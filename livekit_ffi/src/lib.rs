//! Dispatch to the real LiveKit backend or a stub based on the `with_livekit` feature.

#[cfg(feature = "with_livekit")]
mod backend { pub use super::backend_livekit::*; }
#[cfg(not(feature = "with_livekit"))]
mod backend { pub use super::backend_stub::*; }

#[cfg(feature = "with_livekit")]
mod backend_livekit;
#[cfg(not(feature = "with_livekit"))]
mod backend_stub;

pub use backend::*;
