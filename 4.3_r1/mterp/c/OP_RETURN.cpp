HANDLE_OPCODE($opcode /*vAA*/)
    vsrc1 = INST_AA(inst);
    ILOGV("|return%s v%d",
        (INST_INST(inst) == OP_RETURN) ? "" : "-object", vsrc1);
    retval.i = GET_REGISTER(vsrc1);
#if defined(LOCCS_DIAOS)
	if ( INST_INST(inst) != OP_RETURN )
    		diaos_monitor_object(curMethod, (Object*)GET_REGISTER(vsrc1));
#endif
    GOTO_returnFromMethod();
OP_END
