//Taken from https://raw.githubusercontent.com/libsdl-org/SDL/e2a5c1d203ba2ab3917075bcd31e4b81b3a85676/src/stdlib/SDL_stdlib.c

#ifdef _M_IX86 // use this file only for 32-bit architecture

extern "C"
{
    /* Float to long */
    void
    __declspec(naked)
    _ftol()
    {
        /* *INDENT-OFF* */
        __asm {
            push        ebp
            mov         ebp,esp
            sub         esp,20h
            and         esp,0FFFFFFF0h
            fld         st(0)
            fst         dword ptr [esp+18h]
            fistp       qword ptr [esp+10h]
            fild        qword ptr [esp+10h]
            mov         edx,dword ptr [esp+18h]
            mov         eax,dword ptr [esp+10h]
            test        eax,eax
            je          integer_QnaN_or_zero
    arg_is_not_integer_QnaN:
            fsubp       st(1),st
            test        edx,edx
            jns         positive
            fstp        dword ptr [esp]
            mov         ecx,dword ptr [esp]
            xor         ecx,80000000h
            add         ecx,7FFFFFFFh
            adc         eax,0
            mov         edx,dword ptr [esp+14h]
            adc         edx,0
            jmp         localexit
    positive:
            fstp        dword ptr [esp]
            mov         ecx,dword ptr [esp]
            add         ecx,7FFFFFFFh
            sbb         eax,0
            mov         edx,dword ptr [esp+14h]
            sbb         edx,0
            jmp         localexit
    integer_QnaN_or_zero:
            mov         edx,dword ptr [esp+14h]
            test        edx,7FFFFFFFh
            jne         arg_is_not_integer_QnaN
            fstp        dword ptr [esp+18h]
            fstp        dword ptr [esp+18h]
    localexit:
            leave
            ret
        }
        /* *INDENT-ON* */
    }

    void
    _ftol2_sse()
    {
        _ftol();
    }

    void
    __declspec(naked)
    _allshl()
    {
        /* *INDENT-OFF* */
        __asm {
            cmp         cl, 40h
            jae         RETZERO
            cmp         cl, 20h
            jae         MORE32
            shld        edx, eax, cl
            shl         eax, cl
            ret
            MORE32 :
            mov         edx, eax
                xor eax, eax
                and cl, 1Fh
                shl         edx, cl
                ret
                RETZERO :
            xor eax, eax
                xor edx, edx
                ret
        }
        /* *INDENT-ON* */
    }

    void
    __declspec(naked)
    _aullshr()
    {
        /* *INDENT-OFF* */
        __asm {
            cmp         cl, 40h
            jae         RETZERO
            cmp         cl, 20h
            jae         MORE32
            shrd        eax, edx, cl
            shr         edx, cl
            ret
            MORE32 :
            mov         eax, edx
                xor edx, edx
                and cl, 1Fh
                shr         eax, cl
                ret
                RETZERO :
            xor eax, eax
                xor edx, edx
                ret
        }
        /* *INDENT-ON* */
    }

    /* 64-bit math operators for 32-bit systems */
    void
    __declspec(naked)
    _allmul()
    {
        /* *INDENT-OFF* */
        __asm {
            mov         eax, dword ptr[esp+8]
            mov         ecx, dword ptr[esp+10h]
            or          ecx, eax
            mov         ecx, dword ptr[esp+0Ch]
            jne         hard
            mov         eax, dword ptr[esp+4]
            mul         ecx
            ret         10h
    hard:
            push        ebx
            mul         ecx
            mov         ebx, eax
            mov         eax, dword ptr[esp+8]
            mul         dword ptr[esp+14h]
            add         ebx, eax
            mov         eax, dword ptr[esp+8]
            mul         ecx
            add         edx, ebx
            pop         ebx
            ret         10h
        }
        /* *INDENT-ON* */
    }
}

#endif