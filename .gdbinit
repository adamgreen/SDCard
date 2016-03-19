define dump-sdlog
    if ($argc == 1)
        set var $sd_log=$arg0.m_log
        if ($sd_log.m_pDequeue < $sd_log.m_pEnqueue)
            dump-sized-string $sd_log.m_pDequeue ($sd_log.m_pEnqueue-$sd_log.m_pDequeue)
        else
            dump-sized-string $sd_log.m_pDequeue ($sd_log.m_pEnd-$sd_log.m_pDequeue)
            dump-sized-string $sd_log.m_pStart ($sd_log.m_pEnqueue-$sd_log.m_pStart)
        end
    else
        printf "Requires one argument, name of SDFileSystem object to dump\n"
    end
end

# Take pointer and length to dump...dump a character at a time.
# Used internally by dump-sdlog macro.
define dump-sized-string
    set var $pCurr=(const char*)($arg0)
    set var $len=(unsigned int)($arg1)
    while ($len > 0)
        printf "%c", *$pCurr
        set var $pCurr=$pCurr + 1
        set var $len=$len-1
    end
end

document dump-sdlog
Dumps the current content of a SDFileSystem error log.

Requires one argument, the name of the SDFileSystem object to dump.
end
