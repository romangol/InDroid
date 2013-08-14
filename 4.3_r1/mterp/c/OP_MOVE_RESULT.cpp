HANDLE_OPCODE($opcode /*vAA*/)
    vdst = INST_AA(inst);
    ILOGV("|move-result%s v%d %s(v%d=0x%08x)",
         (INST_INST(inst) == OP_MOVE_RESULT) ? "" : "-object",
         vdst, kSpacing+4, vdst,retval.i);
    SET_REGISTER(vdst, retval.i);
#if defined(LOCCS_DIAOS)
	if ( INST_INST(inst) != OP_MOVE_RESULT )
    		diaos_monitor_object(curMethod, (Object*)GET_REGISTER(vdst));
#endif 
    FINISH(1);
OP_END
