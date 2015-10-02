#include "protocol.h"

using namespace std;

namespace MSG {
	map<Type, Factory::Constructor> Factory::constructors;
	map<type_index, Type> Factory::types;

	static FactoryRecord<InfoReq> InfoReq_Recorder(Type::INFO_REQ);
	static FactoryRecord<InfoResp> InfoResp_Recorder(Type::INFO_RESP);
	static FactoryRecord<DiffReq> DiffReq_Recorder(Type::DIFF_REQ);
	static FactoryRecord<DiffResp> DiffResp_Recorder(Type::DIFF_RESP);
	static FactoryRecord<DiffCommit> DiffCommit_Recorder(Type::DIFF_COMMIT);
	static FactoryRecord<XfrEstablishReq> XfrEstablishReq_Recorder(Type::XFR_ESTABLISH_REQ);
	static FactoryRecord<XfrBlock> XfrBlock_Recorder(Type::XFR_BLOCK);
	static FactoryRecord<SyncEstablishReq> SyncEstablishReq_Recorder(Type::SYNC_ESTABLISH_REQ);
	static FactoryRecord<FullsyncCmd> FullsyncCmd_Recorder(Type::FULLSYNC_CMD);
	static FactoryRecord<FlushCmd> FlushCmd_Recorder(Type::FLUSH_CMD);
	static FactoryRecord<InspectReq> InspectReq_Recorder(Type::INSPECT_REQ);
	static FactoryRecord<InspectResp> InspectResp_Recorder(Type::INSPECT_RESP);
}
