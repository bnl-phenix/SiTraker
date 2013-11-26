# Author: Andrey Sukhanov, 04/11/2007
include Makefile.arch
#------------------------------------------------------------------------------
dqO         = Tdq.$(ObjSuf) TdqDict.$(ObjSuf)
dqS         = Tdq.$(SrcSuf) TdqDict.$(SrcSuf)
dqSO        = Tdq.$(DllSuf)

OBJS          = $(dqO)
PROGRAMS      = $(dqSO)

.SUFFIXES: .$(SrcSuf) .$(ObjSuf) .$(DllSuf)

all:            $(PROGRAMS)

Tdq:           $(dqSO)
$(dqSO):       $(dqO)
		$(LD) $(SOFLAGS) $(LDFLAGS) $^ $(EXPLLINKLIBS) $(OutPutOpt)$@
		@echo "$@ done"

clean:
		@rm -f $(OBJS) core

distclean:      clean
		-@mv -f linearIO.root linearIO.roott
		@rm -f $(PROGRAMS) *Dict.* *.def *.exp \
		   *.root *.ps *.so *.lib *.dll *.d .def so_locations
		@rm -rf cxx_repository
		-@mv -f linearIO.roott linearIO.root
		-@cd RootShower && $(MAKE) distclean

.SUFFIXES: .$(SrcSuf)

###

Tdq.$(ObjSuf): Tdq.h
TdqDict.$(SrcSuf): Tdq.h
	@echo "Generating dictionary $@..."
	@rootcint -f $@ -c $^

.$(SrcSuf).$(ObjSuf):
	$(CXX) $(CXXFLAGS) -c $<

