/*
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "RtpsSampleHeader.h"
#include "RtpsUdpInst.h"
#include "RtpsUdpLoader.h"
#include "RtpsUdpTransport.h"
#include "RtpsUdpSendStrategy.h"

#include <dds/DCPS/LogAddr.h>
#include <dds/DCPS/NetworkResource.h>
#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/transport/framework/TransportDefs.h>
#include <dds/DCPS/RTPS/MessageUtils.h>

#include <ace/Configuration.h>

#include <cstring>

OPENDDS_BEGIN_VERSIONED_NAMESPACE_DECL

namespace OpenDDS {
namespace DCPS {

RtpsUdpInst::RtpsUdpInst(const OPENDDS_STRING& name,
                         bool is_template)
  : TransportInst("rtps_udp", name, is_template)
  , send_buffer_size_(*this, &RtpsUdpInst::send_buffer_size, &RtpsUdpInst::send_buffer_size)
  , rcv_buffer_size_(*this, &RtpsUdpInst::rcv_buffer_size, &RtpsUdpInst::rcv_buffer_size)
  , use_multicast_(*this, &RtpsUdpInst::use_multicast, &RtpsUdpInst::use_multicast)
  , ttl_(*this, &RtpsUdpInst::ttl, &RtpsUdpInst::ttl)
  , multicast_interface_(*this, &RtpsUdpInst::multicast_interface, &RtpsUdpInst::multicast_interface)
  , anticipated_fragments_(*this, &RtpsUdpInst::anticipated_fragments, &RtpsUdpInst::anticipated_fragments)
  , max_message_size_(*this, &RtpsUdpInst::max_message_size, &RtpsUdpInst::max_message_size)
  , nak_depth_(*this, &RtpsUdpInst::nak_depth, &RtpsUdpInst::nak_depth)
  , nak_response_delay_(*this, &RtpsUdpInst::nak_response_delay, &RtpsUdpInst::nak_response_delay)
  , heartbeat_period_(*this, &RtpsUdpInst::heartbeat_period, &RtpsUdpInst::heartbeat_period)
  , receive_address_duration_(*this, &RtpsUdpInst::receive_address_duration, &RtpsUdpInst::receive_address_duration)
  , responsive_mode_(*this, &RtpsUdpInst::responsive_mode, &RtpsUdpInst::responsive_mode)
  , send_delay_(*this, &RtpsUdpInst::send_delay, &RtpsUdpInst::send_delay)
  , opendds_discovery_guid_(GUID_UNKNOWN)
  , actual_local_address_(NetworkAddress::default_IPV4)
#ifdef ACE_HAS_IPV6
  , ipv6_actual_local_address_(NetworkAddress::default_IPV6)
#endif
{}

void
RtpsUdpInst::send_buffer_size(ACE_INT32 sbs)
{
  TheServiceParticipant->config_store()->set_int32(config_key("SEND_BUFFER_SIZE").c_str(), sbs);
}

ACE_INT32
RtpsUdpInst::send_buffer_size() const
{
  return TheServiceParticipant->config_store()->get_int32(config_key("SEND_BUFFER_SIZE").c_str(),
#if defined (ACE_DEFAULT_MAX_SOCKET_BUFSIZ)
                                                          ACE_DEFAULT_MAX_SOCKET_BUFSIZ
#else
                                                          0
#endif
                                                          );
}

void
RtpsUdpInst::rcv_buffer_size(ACE_INT32 sbs)
{
  TheServiceParticipant->config_store()->set_int32(config_key("RCV_BUFFER_SIZE").c_str(), sbs);
}

ACE_INT32
RtpsUdpInst::rcv_buffer_size() const
{
  return TheServiceParticipant->config_store()->get_int32(config_key("RCV_BUFFER_SIZE").c_str(),
#if defined (ACE_DEFAULT_MAX_SOCKET_BUFSIZ)
                                                          ACE_DEFAULT_MAX_SOCKET_BUFSIZ
#else
                                                          0
#endif
                                                          );
}

void
RtpsUdpInst::use_multicast(bool um)
{
  TheServiceParticipant->config_store()->set_boolean(config_key("USE_MULTICAST").c_str(), um);
}

bool
RtpsUdpInst::use_multicast() const
{
  return TheServiceParticipant->config_store()->get_boolean(config_key("USE_MULTICAST").c_str(), true);
}

void
RtpsUdpInst::ttl(unsigned char t)
{
  TheServiceParticipant->config_store()->set_uint32(config_key("TTL").c_str(), t);
}

unsigned char
RtpsUdpInst::ttl() const
{
  return TheServiceParticipant->config_store()->get_uint32(config_key("TTL").c_str(), 1);
}

void
RtpsUdpInst::multicast_interface(const String& mi)
{
  TheServiceParticipant->config_store()->set(config_key("MULTICAST_INTERFACE").c_str(), mi);
}

String
RtpsUdpInst::multicast_interface() const
{
  const String d = (TheServiceParticipant->default_address() != NetworkAddress::default_IPV4) ?
    DCPS::LogAddr::ip(TheServiceParticipant->default_address().to_addr()) : "";
  return TheServiceParticipant->config_store()->get(config_key("MULTICAST_INTERFACE").c_str(), d);
}

void
RtpsUdpInst::anticipated_fragments(size_t af)
{
  TheServiceParticipant->config_store()->set_uint32(config_key("ANTICIPATED_FRAGMENTS").c_str(), static_cast<DDS::UInt32>(af));
}

size_t
RtpsUdpInst::anticipated_fragments() const
{
  return TheServiceParticipant->config_store()->get_uint32(config_key("ANTICIPATED_FRAGMENTS").c_str(), RtpsUdpSendStrategy::UDP_MAX_MESSAGE_SIZE / RtpsSampleHeader::FRAG_SIZE);
}

void
RtpsUdpInst::max_message_size(size_t mms)
{
  TheServiceParticipant->config_store()->set_uint32(config_key("MAX_MESSAGE_SIZE").c_str(), static_cast<DDS::UInt32>(mms));
}

size_t
RtpsUdpInst::max_message_size() const
{
  return TheServiceParticipant->config_store()->get_uint32(config_key("MAX_MESSAGE_SIZE").c_str(), RtpsUdpSendStrategy::UDP_MAX_MESSAGE_SIZE);
}

void
RtpsUdpInst::nak_depth(size_t mms)
{
  TheServiceParticipant->config_store()->set_uint32(config_key("NAK_DEPTH").c_str(), static_cast<DDS::UInt32>(mms));
}

size_t
RtpsUdpInst::nak_depth() const
{
  return TheServiceParticipant->config_store()->get_uint32(config_key("NAK_DEPTH").c_str(), 0);
}

void
RtpsUdpInst::nak_response_delay(const TimeDuration& nrd)
{
  TheServiceParticipant->config_store()->set(config_key("NAK_RESPONSE_DELAY").c_str(),
                                             nrd,
                                             ConfigStoreImpl::Format_IntegerMilliseconds);
}

TimeDuration
RtpsUdpInst::nak_response_delay() const
{
  return TheServiceParticipant->config_store()->get(config_key("NAK_RESPONSE_DELAY").c_str(),
                                                    TimeDuration(0, DEFAULT_NAK_RESPONSE_DELAY_USEC),
                                                    ConfigStoreImpl::Format_IntegerMilliseconds);
}

void
RtpsUdpInst::heartbeat_period(const TimeDuration& hp)
{
  TheServiceParticipant->config_store()->set(config_key("HEARTBEAT_PERIOD").c_str(),
                                             hp,
                                             ConfigStoreImpl::Format_IntegerMilliseconds);
}

TimeDuration
RtpsUdpInst::heartbeat_period() const
{
  return TheServiceParticipant->config_store()->get(config_key("HEARTBEAT_PERIOD").c_str(),
                                                    TimeDuration(DEFAULT_HEARTBEAT_PERIOD_SEC, 0),
                                                    ConfigStoreImpl::Format_IntegerMilliseconds);
}

void
RtpsUdpInst::receive_address_duration(const TimeDuration& rad)
{
  TheServiceParticipant->config_store()->set(config_key("RECEIVE_ADDRESS_DURATION").c_str(),
                                             rad,
                                             ConfigStoreImpl::Format_IntegerMilliseconds);
}

TimeDuration
RtpsUdpInst::receive_address_duration() const
{
  return TheServiceParticipant->config_store()->get(config_key("RECEIVE_ADDRESS_DURATION").c_str(),
                                                    TimeDuration(5, 0),
                                                    ConfigStoreImpl::Format_IntegerMilliseconds);
}

void
RtpsUdpInst::responsive_mode(bool rm)
{
  TheServiceParticipant->config_store()->set_boolean(config_key("RESPONSIVE_MODE").c_str(), rm);
}

bool
RtpsUdpInst::responsive_mode() const
{
  return TheServiceParticipant->config_store()->get_boolean(config_key("RESPONSIVE_MODE").c_str(), false);
}

void
RtpsUdpInst::send_delay(const TimeDuration& sd)
{
  TheServiceParticipant->config_store()->set(config_key("SEND_DELAY").c_str(),
                                             sd,
                                             ConfigStoreImpl::Format_IntegerMilliseconds);
}

TimeDuration
RtpsUdpInst::send_delay() const
{
  return TheServiceParticipant->config_store()->get(config_key("SEND_DELAY").c_str(),
                                                    TimeDuration(0, 10 * 1000),
                                                    ConfigStoreImpl::Format_IntegerMilliseconds);
}

void
RtpsUdpInst::multicast_group_address(const NetworkAddress& addr)
{
  TheServiceParticipant->config_store()->set(config_key("MULTICAST_GROUP_ADDRESS").c_str(),
                                             addr,
                                             ConfigStoreImpl::Format_Optional_Port,
                                             ConfigStoreImpl::Kind_IPV4);
}

NetworkAddress
RtpsUdpInst::multicast_group_address() const
{
  return TheServiceParticipant->config_store()->get(config_key("MULTICAST_GROUP_ADDRESS").c_str(),
                                                    NetworkAddress(7401, "239.255.0.2"),
                                                    ConfigStoreImpl::Format_Optional_Port,
                                                    ConfigStoreImpl::Kind_IPV4);
}

void
RtpsUdpInst::local_address(const NetworkAddress& addr)
{
  TheServiceParticipant->config_store()->set(config_key("LOCAL_ADDRESS").c_str(),
                                             addr,
                                             ConfigStoreImpl::Format_Required_Port,
                                             ConfigStoreImpl::Kind_IPV4);
}

NetworkAddress
RtpsUdpInst::local_address() const
{
  return TheServiceParticipant->config_store()->get(config_key("LOCAL_ADDRESS").c_str(),
                                                    TheServiceParticipant->default_address(),
                                                    ConfigStoreImpl::Format_Required_Port,
                                                    ConfigStoreImpl::Kind_IPV4);
}

void
RtpsUdpInst::advertised_address(const NetworkAddress& addr)
{
  TheServiceParticipant->config_store()->set(config_key("ADVERTISED_ADDRESS").c_str(),
                                             addr,
                                             ConfigStoreImpl::Format_Required_Port,
                                             ConfigStoreImpl::Kind_IPV4);
}

NetworkAddress
RtpsUdpInst::advertised_address() const
{
  return TheServiceParticipant->config_store()->get(config_key("ADVERTISED_ADDRESS").c_str(),
                                                    NetworkAddress::default_IPV4,
                                                    ConfigStoreImpl::Format_Required_Port,
                                                    ConfigStoreImpl::Kind_IPV4);
}

#ifdef ACE_HAS_IPV6
void
RtpsUdpInst::ipv6_multicast_group_address(const NetworkAddress& addr)
{
  TheServiceParticipant->config_store()->set(config_key("IPV6_MULTICAST_GROUP_ADDRESS").c_str(),
                                             addr,
                                             ConfigStoreImpl::Format_Optional_Port,
                                             ConfigStoreImpl::Kind_IPV6);
}

NetworkAddress
RtpsUdpInst::ipv6_multicast_group_address() const
{
  return TheServiceParticipant->config_store()->get(config_key("IPV6_MULTICAST_GROUP_ADDRESS").c_str(),
                                                    NetworkAddress(7401, "FF03::2"),
                                                    ConfigStoreImpl::Format_Optional_Port,
                                                    ConfigStoreImpl::Kind_IPV6);
}

void
RtpsUdpInst::ipv6_local_address(const NetworkAddress& addr)
{
  TheServiceParticipant->config_store()->set(config_key("IPV6_LOCAL_ADDRESS").c_str(),
                                             addr,
                                             ConfigStoreImpl::Format_Required_Port,
                                             ConfigStoreImpl::Kind_IPV6);
}

NetworkAddress
RtpsUdpInst::ipv6_local_address() const
{
  return TheServiceParticipant->config_store()->get(config_key("IPV6_LOCAL_ADDRESS").c_str(),
                                                    NetworkAddress::default_IPV6,
                                                    ConfigStoreImpl::Format_Required_Port,
                                                    ConfigStoreImpl::Kind_IPV6);
}

void
RtpsUdpInst::ipv6_advertised_address(const NetworkAddress& addr)
{
  TheServiceParticipant->config_store()->set(config_key("IPV6_ADVERTISED_ADDRESS").c_str(),
                                             addr,
                                             ConfigStoreImpl::Format_Required_Port,
                                             ConfigStoreImpl::Kind_IPV6);
}

NetworkAddress
RtpsUdpInst::ipv6_advertised_address() const
{
  return TheServiceParticipant->config_store()->get(config_key("IPV6_ADVERTISED_ADDRESS").c_str(),
                                                    NetworkAddress::default_IPV6,
                                                    ConfigStoreImpl::Format_Required_Port,
                                                    ConfigStoreImpl::Kind_IPV6);
}
#endif

void
RtpsUdpInst::rtps_relay_only(bool flag)
{
  TheServiceParticipant->config_store()->set_boolean(config_key("RTPS_RELAY_ONLY").c_str(), flag);
}

bool
RtpsUdpInst::rtps_relay_only() const
{
  return TheServiceParticipant->config_store()->get_boolean(config_key("RTPS_RELAY_ONLY").c_str(), false);
}

void
RtpsUdpInst::use_rtps_relay(bool flag)
{
  TheServiceParticipant->config_store()->set_boolean(config_key("USE_RTPS_RELAY").c_str(), flag);
}

bool
RtpsUdpInst::use_rtps_relay() const
{
  return TheServiceParticipant->config_store()->get_boolean(config_key("USE_RTPS_RELAY").c_str(), false);
}

void
RtpsUdpInst::rtps_relay_address(const NetworkAddress& address)
{
  TheServiceParticipant->config_store()->set(config_key("DATA_RTPS_RELAY_ADDRESS").c_str(),
                                             address,
                                             ConfigStoreImpl::Format_Required_Port,
                                             ConfigStoreImpl::Kind_IPV4);
}

NetworkAddress
RtpsUdpInst::rtps_relay_address() const
{
  return TheServiceParticipant->config_store()->get(config_key("DATA_RTPS_RELAY_ADDRESS").c_str(),
                                                    NetworkAddress::default_IPV4,
                                                    ConfigStoreImpl::Format_Required_Port,
                                                    ConfigStoreImpl::Kind_IPV4);
}

void
RtpsUdpInst::use_ice(bool flag)
{
  TheServiceParticipant->config_store()->set_boolean(config_key("USE_ICE").c_str(), flag);
}

bool
RtpsUdpInst::use_ice() const
{
  return TheServiceParticipant->config_store()->get_boolean(config_key("USE_ICE").c_str(), false);
}

void
RtpsUdpInst::stun_server_address(const NetworkAddress& address)
{
  TheServiceParticipant->config_store()->set(config_key("DATA_STUN_SERVER_ADDRESS").c_str(),
                                             address,
                                             ConfigStoreImpl::Format_Required_Port,
                                             ConfigStoreImpl::Kind_IPV4);
}

NetworkAddress
RtpsUdpInst::stun_server_address() const
{
  return TheServiceParticipant->config_store()->get(config_key("DATA_STUN_SERVER_ADDRESS").c_str(),
                                                    NetworkAddress::default_IPV4,
                                                    ConfigStoreImpl::Format_Required_Port,
                                                    ConfigStoreImpl::Kind_IPV4);
}

TransportImpl_rch
RtpsUdpInst::new_impl()
{
  return make_rch<RtpsUdpTransport>(rchandle_from(this));
}

int
RtpsUdpInst::load(ACE_Configuration_Heap& cf,
                  ACE_Configuration_Section_Key& sect)
{
  TransportInst::load(cf, sect); // delegate to parent
  return 0;
}

OPENDDS_STRING
RtpsUdpInst::dump_to_str() const
{
  OPENDDS_STRING ret = TransportInst::dump_to_str();
  ret += formatNameForDump("send_buffer_size") + to_dds_string(send_buffer_size()) + '\n';
  ret += formatNameForDump("rcv_buffer_size") + to_dds_string(rcv_buffer_size()) + '\n';
  ret += formatNameForDump("use_multicast") + (use_multicast() ? "true" : "false") + '\n';
  ret += formatNameForDump("ttl") + to_dds_string(ttl()) + '\n';
  ret += formatNameForDump("multicast_interface") + multicast_interface() + '\n';
  ret += formatNameForDump("anticipated_fragments") + to_dds_string(unsigned(anticipated_fragments())) + '\n';
  ret += formatNameForDump("max_message_size") + to_dds_string(unsigned(max_message_size())) + '\n';
  ret += formatNameForDump("nak_depth") + to_dds_string(unsigned(nak_depth())) + '\n';
  ret += formatNameForDump("nak_response_delay") + nak_response_delay().str() + '\n';
  ret += formatNameForDump("heartbeat_period") + heartbeat_period().str() + '\n';
  ret += formatNameForDump("responsive_mode") + (responsive_mode() ? "true" : "false") + '\n';
  ret += formatNameForDump("multicast_group_address") + LogAddr(multicast_group_address()).str() + '\n';
  ret += formatNameForDump("local_address") + LogAddr(local_address()).str() + '\n';
  ret += formatNameForDump("advertised_address") + LogAddr(advertised_address()).str() + '\n';
#ifdef ACE_HAS_IPV6
  ret += formatNameForDump("ipv6_multicast_group_address") + LogAddr(ipv6_multicast_group_address()).str() + '\n';
  ret += formatNameForDump("ipv6_local_address") + LogAddr(ipv6_local_address()).str() + '\n';
  ret += formatNameForDump("ipv6_advertised_address") + LogAddr(ipv6_advertised_address()).str() + '\n';
#endif
  return ret;
}

size_t
RtpsUdpInst::populate_locator(TransportLocator& info, ConnectionInfoFlags flags) const
{
  using namespace OpenDDS::RTPS;

  LocatorSeq locators;
  CORBA::ULong idx = 0;

  // multicast first so it's preferred by remote peers
  const NetworkAddress multicast_group_addr = multicast_group_address();
  if ((flags & CONNINFO_MULTICAST) && use_multicast() && multicast_group_addr != NetworkAddress::default_IPV4) {
    grow(locators);
    address_to_locator(locators[idx++], multicast_group_addr.to_addr());
  }
#ifdef ACE_HAS_IPV6
  const NetworkAddress ipv6_multicast_group_addr = ipv6_multicast_group_address();
  if ((flags & CONNINFO_MULTICAST) && use_multicast() && ipv6_multicast_group_addr != NetworkAddress::default_IPV6) {
    grow(locators);
    address_to_locator(locators[idx++], ipv6_multicast_group_addr.to_addr());
  }
#endif

  if (flags & CONNINFO_UNICAST) {
    const NetworkAddress addr = (actual_local_address_ == NetworkAddress::default_IPV4) ? local_address() : actual_local_address_;
    if (addr != NetworkAddress::default_IPV4) {
      if (advertised_address() != NetworkAddress::default_IPV4) {
        grow(locators);
        address_to_locator(locators[idx], advertised_address().to_addr());
        if (locators[idx].port == 0) {
          locators[idx].port = addr.get_port_number();
        }
        ++idx;
      } else if (addr.is_any()) {
        typedef OPENDDS_VECTOR(ACE_INET_Addr) AddrVector;
        AddrVector addrs;
        get_interface_addrs(addrs);
        for (AddrVector::iterator adr_it = addrs.begin(); adr_it != addrs.end(); ++adr_it) {
          if (*adr_it != ACE_INET_Addr() && adr_it->get_type() == AF_INET) {
            grow(locators);
            address_to_locator(locators[idx], *adr_it);
            locators[idx].port = addr.get_port_number();
            ++idx;
          }
        }
      } else {
        grow(locators);
        address_to_locator(locators[idx++], addr.to_addr());
      }
    }
#ifdef ACE_HAS_IPV6
    const NetworkAddress addr6 = (ipv6_actual_local_address_ == NetworkAddress::default_IPV6) ? ipv6_local_address() : ipv6_actual_local_address_;
    if (addr6 != NetworkAddress::default_IPV6) {
      if (ipv6_advertised_address() != NetworkAddress::default_IPV6) {
        grow(locators);
        address_to_locator(locators[idx], ipv6_advertised_address().to_addr());
        if (locators[idx].port == 0) {
          locators[idx].port = addr6.get_port_number();
        }
        ++idx;
      } else if (addr6.is_any()) {
        typedef OPENDDS_VECTOR(ACE_INET_Addr) AddrVector;
        AddrVector addrs;
        get_interface_addrs(addrs);
        for (AddrVector::iterator adr_it = addrs.begin(); adr_it != addrs.end(); ++adr_it) {
          if (*adr_it != ACE_INET_Addr() && adr_it->get_type() == AF_INET6) {
            grow(locators);
            address_to_locator(locators[idx], *adr_it);
            locators[idx].port = addr6.get_port_number();
            ++idx;
          }
        }
      } else {
        grow(locators);
        address_to_locator(locators[idx++], addr6.to_addr());
      }
    }
#endif
  }

  info.transport_type = "rtps_udp";
  RTPS::locators_to_blob(locators, info.data);

  return locators.length();
}

const TransportBLOB*
RtpsUdpInst::get_blob(const TransportLocatorSeq& trans_info) const
{
  for (CORBA::ULong idx = 0, limit = trans_info.length(); idx != limit; ++idx) {
    if (std::strcmp(trans_info[idx].transport_type, "rtps_udp") == 0) {
      return &trans_info[idx].data;
    }
  }

  return 0;
}

void
RtpsUdpInst::update_locators(const GUID_t& remote_id,
                             const TransportLocatorSeq& locators)
{
  TransportImpl_rch imp = get_or_create_impl();
  if (imp) {
    RtpsUdpTransport_rch rtps_impl = static_rchandle_cast<RtpsUdpTransport>(imp);
    rtps_impl->update_locators(remote_id, locators);
  }
}

void
RtpsUdpInst::get_last_recv_locator(const GUID_t& remote_id,
                                   TransportLocator& locator)
{
  TransportImpl_rch imp = get_or_create_impl();
  if (imp) {
    RtpsUdpTransport_rch rtps_impl = static_rchandle_cast<RtpsUdpTransport>(imp);
    rtps_impl->get_last_recv_locator(remote_id, locator);
  }
}

void
RtpsUdpInst::append_transport_statistics(TransportStatisticsSequence& seq)
{
  TransportImpl_rch imp = get_or_create_impl();
  if (imp) {
    RtpsUdpTransport_rch rtps_impl = static_rchandle_cast<RtpsUdpTransport>(imp);
    rtps_impl->append_transport_statistics(seq);
  }
}

} // namespace DCPS
} // namespace OpenDDS

OPENDDS_END_VERSIONED_NAMESPACE_DECL
