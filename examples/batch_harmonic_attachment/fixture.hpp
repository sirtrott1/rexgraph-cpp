#pragma once
#include "rfc/inference/native_batch_harmonic_attachment.hpp"
#include "../native_frames/fixture.hpp"

namespace batch_attachment_example {
inline std::vector<rfc::core::BatchAttachmentCell> declarations() {
    using namespace rfc;
    return {{core::RelationKey{200},0,{{0,true,false,1},{1,true,false,1},{2,true,false,1},{3,true,false,1}}},
        {core::RelationKey{201},0,{{0,true,false,2},{1,true,false,2},{2,true,false,2},{3,true,false,2}}},
        {core::RelationKey{202},0,{{0,true,false,1},{0,true,true,1}}}};
}
inline rfc::storage::TemporalEndpoint endpoint() {
    return native_frame_example::endpoint(native_green_example::square().first.source,2);
}
inline rfc::inference::DerivedNativeBatchHodgeDecomposition run() {
    using namespace rfc;
    storage::NativeAccessionDirectory directory(endpoint(),900);const runtime::WorkAvailability available{900,7};
    const core::Grade grade{1};const auto input=native_frame_example::input(directory,grade);
    const auto frames=inference::PreparedNativeHodgeFrames::prepare(input,native_frame_example::request(directory,grade,2),available);
    const auto plan=inference::PreparedNativeBatchHarmonicAttachment::prepare(frames,{declarations(),64},available);
    return plan.execute(input,available);
}
} // namespace batch_attachment_example
