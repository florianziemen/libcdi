#ifndef WIN_ERR_TO_CDI_ERR_H
#define WIN_ERR_TO_CDI_ERR_H

int
translate_windows_error_to_errno(DWORD win_error)
{
    switch (win_error)
    {
        case ERROR_INVALID_PARAMETER:
        case ERROR_INVALID_FUNCTION:
        case ERROR_BAD_ARGUMENTS:
        case ERROR_NEGATIVE_SEEK:
        case ERROR_SEEK:
            return EINVAL;

        default:
            return EIO;
    }
}

#endif
