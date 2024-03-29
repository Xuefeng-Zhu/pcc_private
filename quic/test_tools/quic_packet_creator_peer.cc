// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_packet_creator_peer.h"

#include "net/quic/quic_packet_creator.h"

namespace net {
namespace test {

// static
bool QuicPacketCreatorPeer::SendVersionInPacket(QuicPacketCreator* creator) {
  return creator->send_version_in_packet_;
}

// static
void QuicPacketCreatorPeer::SetSendVersionInPacket(
    QuicPacketCreator* creator,
    bool send_version_in_packet) {
  creator->send_version_in_packet_ = send_version_in_packet;
}

// static
void QuicPacketCreatorPeer::SetPacketNumberLength(
    QuicPacketCreator* creator,
    QuicPacketNumberLength packet_number_length) {
  creator->packet_number_length_ = packet_number_length;
}

// static
void QuicPacketCreatorPeer::SetNextPacketNumberLength(
    QuicPacketCreator* creator,
    QuicPacketNumberLength next_packet_number_length) {
  creator->next_packet_number_length_ = next_packet_number_length;
}

// static
QuicPacketNumberLength QuicPacketCreatorPeer::NextPacketNumberLength(
    QuicPacketCreator* creator) {
  return creator->next_packet_number_length_;
}

// static
QuicPacketNumberLength QuicPacketCreatorPeer::GetPacketNumberLength(
    QuicPacketCreator* creator) {
  return creator->packet_number_length_;
}

void QuicPacketCreatorPeer::SetPacketNumber(QuicPacketCreator* creator,
                                            QuicPacketNumber s) {
  creator->packet_number_ = s;
}

// static
void QuicPacketCreatorPeer::FillPacketHeader(QuicPacketCreator* creator,
                                             QuicFecGroupNumber fec_group,
                                             bool fec_flag,
                                             QuicPacketHeader* header) {
  creator->FillPacketHeader(fec_group, fec_flag, header);
}

}  // namespace test
}  // namespace net
