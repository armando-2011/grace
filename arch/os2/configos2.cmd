/* 
  REXX-Script to configure & build GRACE 5.x for XFree86 OS/2
  (19991228)
*/

/*
   Todo:
       - forget about this mess and switch to configure ;-)
       - Ask/check for libhelp, g77, ...
       - distinguish between debug- & production builds
*/

trace n

Parse Arg param

True  = 1
False = 0

options = ''
do while param <> ''
  Parse Var param tp param
  tp = Translate(tp)
  tpr = Right(tp,1)
  if (Left(tp,1) = '-') & (Length(tp) = 2) then
    select
      when tpr = 'D' then
        do
        options = options'D'
        end
      when tpr = 'E' then
        do
        options = options'E'
        end
      when tpr = 'O' then
        do
        options = options'O'
        end
      when (tpr = '?') then
        do
        call ShowHelp
        SIGNAL FIN
        end
      otherwise
        do
        call ShowHelp
        SIGNAL FIN
        end
    end /* select */
end /* do while */

say "configos2.cmd"
say "Options are: "options

cf_make_conf = 'Make.conf'
cf_config_h  = 'config.h'

curdir = Directory()
x11root = Value('X11ROOT', , 'OS2ENVIRONMENT')
if x11root <> '' then
  do
  x11path=x11root'\XFree86'
  newdir = Directory(x11path)
  call Directory(curdir)
  if Translate(newdir) <> Translate(x11path) then
    do
    say 'XFree/2 is not properly installed!'
    say 'The X11ROOT environment variable is not set correctly'
    end
  end
else
  do
  say 'XFree/2 is not properly installed!'
  say 'The X11ROOT environment variable is missing'
  SIGNAL FIN
  end

systemdir = Strip(curdir, 'T', '\')'\arch\os2'

/* Install Make.conf */
if FileExists(cf_make_conf) = True then
  do
  if Pos('E', options) = 0 then
    do
    say cf_make_conf' is already installed!'
    call CharOut , 'Install default file instead ? (y/n) '
    Parse Upper Pull answer
    if answer = 'Y' then
      do
      call FileCopy systemdir'\'cf_make_conf'.os2', curdir'\'cf_make_conf
      call Execute 'touch 'curdir'\'cf_make_conf
      end
    else
       do
       /* not a good idea perhaps ... */
       call Execute 'touch 'curdir'\'cf_make_conf
       end
    end
  end
else 
  do 
  call FileCopy systemdir'\'cf_make_conf'.os2', curdir'\'cf_make_conf
/*  call Execute 'touch 'curdir'\'cf_make_conf */
  end

/* 
  Install config.h
*/
if FileExists(cf_config_h) = True then
  do
  if Pos('E', options) = 0 then
    do
    say cf_config_h' is already installed!'
    call CharOut , 'Install default file instead ? (y/n) '
    Parse Upper Pull answer
    if answer = 'Y' then
       do
       call FileCopy systemdir'\'cf_config_h'.os2', curdir'\'cf_config_h
       call Execute 'touch 'curdir'\'cf_config_h
       end
    else
       do
       /* not a good idea perhaps ... */
       call Execute 'touch 'curdir'\'cf_config_h
       end
    end
  end
else 
  do 
  call FileCopy systemdir'\'cf_config_h'.os2', curdir'\'cf_config_h
/*  call Execute 'touch 'curdir'\'cf_config_h */
  end

/*
  Building dlfcn.a
*/

call Execute "cd .\arch\os2 && x11make.exe -f dlfcn.mak all & cd ..\.."

/* Calling x11make.exe cause make.cmd won't work here
   due to the sh command embedded in the Makefiles*/
say 'Start compiling ...'
call Execute 'x11make'

call FileCopy systemdir'\dotest.cmd', curdir'\examples\dotest.cmd'

say 'configos2.cmd has finished!'

FIN:
exit

/* ######################################################################## */

Execute: procedure
Parse Arg command
Address CMD ''command
return


FileCopy: procedure
Parse Arg par1, par2
call Execute '@copy 'par1' 'par2' >nul'
return


FileExists: procedure expose True False
Parse Arg fe_file
rs = Stream(fe_file, 'C', 'QUERY EXISTS')
if rs = '' then
  return False
else
  return True


ShowHelp:
say 'Valid options for configos2.cmd:'
/*
say " -d : create binaries for debugging"
*/
say " -e : use existing configuration files; don't prompt for using default ones"
/*
say " -o : optimize"
*/
return


rtest:
/* work in progress (hopefully ;-) */

_CR     = D2C(13)
_LF     = D2C(10)
_CRLF   = _CR''_LF
rc = LineIn (rfile,1,0)
rlength  = Chars(rfile)
rcontent = CharIn(rfile, rlength)
call LineOut rfile
rcount = 0
do while rcontent <> ''
  Parse Var rcontent rline (_CRLF) rcontent
  rline = Strip(rline, 'T')
  if rline <> '' then
    do
    rcount = rcount+1
    rnew.rcount = rline
    say rcount'. 'rline
    end
end /* do while */
rnew.0 = rcount
return
