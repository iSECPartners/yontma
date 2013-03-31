MessageIdTypedef=DWORD

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
    Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
    Warning=0x2:STATUS_SEVERITY_WARNING
    Error=0x3:STATUS_SEVERITY_ERROR
    )


FacilityNames=(System=0x0:FACILITY_SYSTEM
    Runtime=0x2:FACILITY_RUNTIME
    Stubs=0x3:FACILITY_STUBS
    Io=0x4:FACILITY_IO_ERROR_CODE
)

LanguageNames=(English=0x409:MSG00409)

; // The following are message definitions.

MessageId=0x1
Severity=Error
Facility=Runtime
SymbolicName=E_YONTMA_SERVICE_NOT_INSTALLED
Language=English
The YoNTMA service is not installed.
.

MessageId=0x2
Severity=Error
Facility=Runtime
SymbolicName=E_YONTMA_OS_DRIVE_NOT_ENCRYPTED
Language=English
BitLocker is not enabled on this computer's OS drive. YoNTMA can only protect
your system when your OS drive is fully encrypted.

If your OS drive is encrypted by a technology that YoNTMA does not detect, use
the --force option to install YoNTMA.
.

MessageId=0x3
Severity=Error
Facility=Runtime
SymbolicName=E_YONTMA_HIBERNATE_NOT_ENABLED
Language=English
Hibernation is not enabled on this system. Hibernation is required for YoNTMA
to function.
.

MessageId=0x4
Severity=Error
Facility=Runtime
SymbolicName=E_YONTMA_INVALID_COMMAND_LINE
Language=English
The command line arguments were incorrect.
.

; // A message file must end with a period on its own line
; // followed by a blank line.

