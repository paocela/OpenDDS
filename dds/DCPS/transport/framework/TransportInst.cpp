/*
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "TransportInst.h"
#include "TransportImpl.h"
#include "TransportExceptions.h"
#include "EntryExit.h"

#include "DCPS/SafetyProfileStreams.h"
#include "DCPS/Service_Participant.h"

#include "ace/Configuration.h"

#include <cstring>
#include <algorithm>

OPENDDS_BEGIN_VERSIONED_NAMESPACE_DECL

namespace OpenDDS {
namespace DCPS {

TransportInst::TransportInst(const char* type,
                             const OPENDDS_STRING& name,
                             bool is_template)
  : transport_type_(type)
  , shutting_down_(false)
  , name_(name)
  , config_prefix_(ConfigPair::canonicalize("OPENDDS_TRANSPORT_" + name_))
  , is_template_(is_template)
{
  DBG_ENTRY_LVL("TransportInst", "TransportInst", 6);
}

TransportInst::~TransportInst()
{
  DBG_ENTRY_LVL("TransportInst","~TransportInst",6);
}

int
TransportInst::load(ACE_Configuration_Heap& cf,
                    ACE_Configuration_Section_Key& sect)
{
  process_section(*TheServiceParticipant->config_store(),
                  ConfigReader_rch(),
                  ConfigReaderListener_rch(),
                  config_prefix_,
                  cf,
                  sect,
                  "",
                  false);

  ACE_TString stringvalue;
  if (cf.get_string_value (sect, ACE_TEXT("passive_connect_duration"), stringvalue) == 0) {
    ACE_DEBUG ((LM_WARNING,
                ACE_TEXT ("(%P|%t) WARNING: passive_connect_duration option ")
                ACE_TEXT ("is deprecated in the transport inst, must be ")
                ACE_TEXT ("defined in transport config.\n")));
  }

  return 0;
}

void
TransportInst::dump() const
{
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("\n(%P|%t) TransportInst::dump() -\n%C"),
             dump_to_str().c_str()));
}

namespace {
  static const int NAME_INDENT(3);
  static const int NAME_WIDTH(30); // Includes ":"
}

OPENDDS_STRING
TransportInst::formatNameForDump(const char* name)
{
  OPENDDS_STRING formatted_name;
  formatted_name.reserve(NAME_INDENT + NAME_WIDTH);
  formatted_name += OPENDDS_STRING(NAME_INDENT, ' ');
  formatted_name += name;
  formatted_name += ":";
  if ((NAME_WIDTH + NAME_INDENT) > formatted_name.length()) {
    formatted_name += OPENDDS_STRING((NAME_WIDTH + NAME_INDENT- formatted_name.length()), ' ');
  }
  return formatted_name;
}

OPENDDS_STRING
TransportInst::dump_to_str() const
{
  OPENDDS_STRING ret;
  ret += formatNameForDump("transport_type")          + transport_type_ + '\n';
  ret += formatNameForDump("name")                    + name_ + '\n';
  ret += formatNameForDump("config_prefix")           + config_prefix_ + '\n';
  ret += formatNameForDump("max_packet_size")         + to_dds_string(unsigned(max_packet_size())) + '\n';
  ret += formatNameForDump("max_samples_per_packet")  + to_dds_string(unsigned(max_samples_per_packet())) + '\n';
  ret += formatNameForDump("optimum_packet_size")     + to_dds_string(unsigned(optimum_packet_size())) + '\n';
  ret += formatNameForDump("thread_per_connection")   + (thread_per_connection() ? "true" : "false") + '\n';
  ret += formatNameForDump("datalink_release_delay")  + to_dds_string(datalink_release_delay()) + '\n';
  ret += formatNameForDump("datalink_control_chunks") + to_dds_string(unsigned(datalink_control_chunks())) + '\n';
  ret += formatNameForDump("fragment_reassembly_timeout") + fragment_reassembly_timeout().str() + '\n';
  ret += formatNameForDump("receive_preallocated_message_blocks") + to_dds_string(unsigned(receive_preallocated_message_blocks())) + '\n';
  ret += formatNameForDump("receive_preallocated_data_blocks") + to_dds_string(unsigned(receive_preallocated_data_blocks())) + '\n';
  return ret;
}

void
TransportInst::max_packet_size(ACE_UINT32 mps)
{
  TheServiceParticipant->config_store()->set_uint32(config_key("MAX_PACKET_SIZE").c_str(), mps);
}

ACE_UINT32
TransportInst::max_packet_size() const
{
  return TheServiceParticipant->config_store()->get_uint32(config_key("MAX_PACKET_SIZE").c_str(), DEFAULT_CONFIG_MAX_PACKET_SIZE);
}

void
TransportInst::max_samples_per_packet(size_t mspp)
{
  TheServiceParticipant->config_store()->set_uint32(config_key("MAX_SAMPLES_PER_PACKET").c_str(), static_cast<DDS::UInt32>(mspp));
}

size_t
TransportInst::max_samples_per_packet() const
{
  // Ensure that the number of samples put into the packet does
  // not exceed the allowed number of io vectors to be sent by the OS.
  const size_t configured = TheServiceParticipant->config_store()->get_uint32(config_key("MAX_SAMPLES_PER_PACKET").c_str(),
                                                                              DEFAULT_CONFIG_MAX_SAMPLES_PER_PACKET);
  size_t retval = configured;

  if ((2 * retval + 1) > MAX_SEND_BLOCKS) {
    retval = (MAX_SEND_BLOCKS + 1) / 2 - 1;
    if (log_level >= LogLevel::Notice) {
      ACE_DEBUG((LM_NOTICE,
                 "(%P|%t) NOTICE: TransportInst::max_samples_per_packet: "
                 "max_samples_per_packet adjusted from %B to %B\n",
                 configured, retval));
    }
  }

  return retval;
}

void
TransportInst::optimum_packet_size(ACE_UINT32 ops)
{
  TheServiceParticipant->config_store()->set_uint32(config_key("OPTIMUM_PACKET_SIZE").c_str(), ops);
}

ACE_UINT32
TransportInst::optimum_packet_size() const
{
  return TheServiceParticipant->config_store()->get_uint32(config_key("OPTIMUM_PACKET_SIZE").c_str(), DEFAULT_CONFIG_OPTIMUM_PACKET_SIZE);
}

void
TransportInst::thread_per_connection(bool tpc)
{
  TheServiceParticipant->config_store()->set_boolean(config_key("THREAD_PER_CONNECTION").c_str(), tpc);
}

bool
TransportInst::thread_per_connection() const
{
  return TheServiceParticipant->config_store()->get_boolean(config_key("THREAD_PER_CONNECTION").c_str(), false);
}

void
TransportInst::datalink_release_delay(long drd)
{
  TheServiceParticipant->config_store()->set_int32(config_key("DATALINK_RELEASE_DELAY").c_str(), drd);
}

long
TransportInst::datalink_release_delay() const
{
  return TheServiceParticipant->config_store()->get_int32(config_key("DATALINK_RELEASE_DELAY").c_str(), DEFAULT_DATALINK_RELEASE_DELAY);
}

void
TransportInst::datalink_control_chunks(size_t dcc)
{
  TheServiceParticipant->config_store()->set_uint32(config_key("DATALINK_CONTROL_CHUNKS").c_str(), static_cast<DDS::UInt32>(dcc));
}

size_t
TransportInst::datalink_control_chunks() const
{
  return TheServiceParticipant->config_store()->get_uint32(config_key("DATALINK_CONTROL_CHUNKS").c_str(), DEFAULT_DATALINK_CONTROL_CHUNKS);
}

void
TransportInst::fragment_reassembly_timeout(const TimeDuration& frt)
{
  TheServiceParticipant->config_store()->set(config_key("FRAGMENT_REASSEMBLY_TIMEOUT").c_str(), frt, ConfigStoreImpl::Format_IntegerMilliseconds);
}

TimeDuration
TransportInst::fragment_reassembly_timeout() const
{
  return TheServiceParticipant->config_store()->get(config_key("FRAGMENT_REASSEMBLY_TIMEOUT").c_str(), TimeDuration(300), ConfigStoreImpl::Format_IntegerMilliseconds);
}

void
TransportInst::receive_preallocated_message_blocks(size_t rpmb)
{
  TheServiceParticipant->config_store()->set_uint32(config_key("RECEIVE_PREALLOCATED_MESSAGE_BLOCKS").c_str(), static_cast<DDS::UInt32>(rpmb));
}

size_t
TransportInst::receive_preallocated_message_blocks() const
{
  return TheServiceParticipant->config_store()->get_uint32(config_key("RECEIVE_PREALLOCATED_MESSAGE_BLOCKS").c_str(), 0);
}

void
TransportInst::receive_preallocated_data_blocks(size_t rpdb)
{
  TheServiceParticipant->config_store()->set_uint32(config_key("RECEIVE_PREALLOCATED_DATA_BLOCKS").c_str(), static_cast<DDS::UInt32>(rpdb));
}

size_t
TransportInst::receive_preallocated_data_blocks() const
{
  return TheServiceParticipant->config_store()->get_uint32(config_key("RECEIVE_PREALLOCATED_DATA_BLOCKS").c_str(), 0);
}

void
TransportInst::drop_messages(bool flag)
{
  TheServiceParticipant->config_store()->set_boolean(config_key("DROP_MESSAGES").c_str(), flag);
}

bool
TransportInst::drop_messages() const
{
  return TheServiceParticipant->config_store()->get_boolean(config_key("DROP_MESSAGES").c_str(), false);
}

void
TransportInst::drop_messages_m(double m)
{
  TheServiceParticipant->config_store()->set_float64(config_key("DROP_MESSAGES_M").c_str(), m);
}

double
TransportInst::drop_messages_m() const
{
  return TheServiceParticipant->config_store()->get_float64(config_key("DROP_MESSAGES_M").c_str(), 0);
}

void
TransportInst::drop_messages_b(double b)
{
  TheServiceParticipant->config_store()->set_float64(config_key("DROP_MESSAGES_B").c_str(), b);
}

double
TransportInst::drop_messages_b() const
{
  return TheServiceParticipant->config_store()->get_float64(config_key("DROP_MESSAGES_B").c_str(), 0);
}

void
TransportInst::count_messages(bool flag)
{
  TheServiceParticipant->config_store()->set_boolean(config_key("COUNT_MESSAGES").c_str(), flag);
}

bool
TransportInst::count_messages() const
{
  return TheServiceParticipant->config_store()->get_boolean(config_key("COUNT_MESSAGES").c_str(), false);
}

void
TransportInst::shutdown()
{
  TransportImpl_rch impl;
  {
    ACE_GUARD(ACE_SYNCH_MUTEX, g, lock_);
    impl_.swap(impl);
    shutting_down_ = true;
  }
  if (impl) {
    impl->shutdown();
  }
}

TransportImpl_rch
TransportInst::get_or_create_impl()
{
  ACE_GUARD_RETURN(ACE_SYNCH_MUTEX, g, lock_, TransportImpl_rch());
  if (!impl_ && !shutting_down_) {
    try {
      impl_ = new_impl();
    } catch (const Transport::UnableToCreate&) {
      return TransportImpl_rch();
    }
  }
  return impl_;
}

TransportImpl_rch
TransportInst::get_impl()
{
  ACE_GUARD_RETURN(ACE_SYNCH_MUTEX, g, lock_, TransportImpl_rch());
  return impl_;
}

void
TransportInst::set_port_in_addr_string(OPENDDS_STRING& addr_str, u_short port_number)
{
#ifdef BUFSIZE
#undef BUFSIZE
#endif
  const int BUFSIZE=1024;
  char result[BUFSIZE];

  if (std::count(addr_str.begin(), addr_str.end(), ':') < 2) {
    OPENDDS_STRING::size_type pos = addr_str.find_last_of(":");
    ACE_OS::snprintf(result, BUFSIZE, "%.*s:%hu", static_cast<int>(pos), addr_str.c_str(), port_number);
  }
  else {
    // this is the numeric form of ipv6 address because it has more than one ':'
    if (addr_str[0] != '[') {
      ACE_OS::snprintf(result, BUFSIZE, "[%s]:%hu", addr_str.c_str(), port_number);
    }
    else {
      OPENDDS_STRING::size_type pos = addr_str.find_last_of("]");
      ACE_OS::snprintf(result, BUFSIZE, "%.*s:%hu", static_cast<int>(pos+1), addr_str.c_str(), port_number);
    }
  }
  addr_str = result;
}

WeakRcHandle<OpenDDS::ICE::Endpoint>
TransportInst::get_ice_endpoint()
{
  const TransportImpl_rch temp = get_or_create_impl();
  return temp ? temp->get_ice_endpoint() : WeakRcHandle<OpenDDS::ICE::Endpoint>();
}

void
TransportInst::rtps_relay_only_now(bool flag)
{
  TheServiceParticipant->config_store()->set_boolean(config_key("RTPS_RELAY_ONLY").c_str(), flag);
}

void
TransportInst::use_rtps_relay_now(bool flag)
{
  TheServiceParticipant->config_store()->set_boolean(config_key("USE_RTPS_RELAY").c_str(), flag);
}

void
TransportInst::use_ice_now(bool flag)
{
  TheServiceParticipant->config_store()->set_boolean(config_key("USE_ICE").c_str(), flag);
}

ReactorTask_rch
TransportInst::reactor_task()
{
  const TransportImpl_rch temp = get_or_create_impl();
  return temp ? temp->reactor_task() : ReactorTask_rch();
}

EventDispatcher_rch
TransportInst::event_dispatcher()
{
  const TransportImpl_rch temp = get_or_create_impl();
  return temp ? temp->event_dispatcher() : EventDispatcher_rch();
}

} // DCPS
} // OpenDDS

OPENDDS_END_VERSIONED_NAMESPACE_DECL
