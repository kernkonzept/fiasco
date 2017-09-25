#ifndef L4_CXX_OF1275_H__
#define L4_CXX_OF1275_H__

#include <stdarg.h>
#include <string.h>

namespace L4 {

	class Of
	{

	private:
		/* declarations */
		struct prom_args 
		{
			const char *service;
			int nargs;
			int nret;
			void *args[10];
			prom_args(const char *s, int na, int nr) : service(s), nargs(na), nret(nr) {}
		};
		
		typedef int (*prom_entry)(struct prom_args *);
		
		/* attributes */
		prom_entry _prom;
		
		//int prom_call(const char *service, int nargs, int nret, ...) const;
	protected:
		enum {
			PROM_ERROR = -1u
		};
		
		
		typedef void *ihandle_t; 
		typedef void *phandle_t;
		
		typedef struct 
		{
			unsigned long len;
			char          data[];
		} of_item_t;

		/* methods */
		unsigned prom_call(const char *service, int nargs, int nret, ...) const
		{
			struct prom_args args = prom_args(service, nargs, nret);
			va_list list;

			va_start(list, nret);
			for(int i = 0; i < nargs; i++)
				args.args[i] = va_arg(list, void*);
			va_end(list);

			for(int i = 0; i < nret; i++)
				args.args[nargs + i] = 0;

			if(_prom(&args) < 0)
				return PROM_ERROR;

			return (nret > 0) ? (int)args.args[nargs] : 0;
		}


		inline int prom_getprop(phandle_t node, const char *pname,  void *value, 
				size_t size)
		{
			return prom_call("getprop", 4, 1, node, pname, (unsigned long)value,
			                (unsigned long)size);
		}

		inline int prom_next_node(phandle_t *nodep)
		{
			phandle_t node;

			if ((node = *nodep) != 0
			    && (*nodep = (phandle_t)prom_call("child", 1, 1, node)) != 0)
				return 1;
			if ((*nodep = (phandle_t)prom_call("peer", 1, 1, node)) != 0)
				return 1;
			for (;;) {
				if ((node = (phandle_t)prom_call("parent", 1, 1, node)) == 0)
					return 0;
				if ((*nodep = (phandle_t)prom_call("peer", 1, 1, node)) != 0)
					return 1;
			}
		}
		
		template<typename T>
		static inline bool handle_valid(T p)
		{
			return ((unsigned)p != 0 && (unsigned)p != PROM_ERROR);
		}
	
	public:
		Of() : _prom(0) {}
		Of(unsigned long prom) : _prom(reinterpret_cast<prom_entry>(prom)) {}
		
		template<typename T>
		inline void set_prom(T prom) 
		{
			_prom = reinterpret_cast<prom_entry>(prom);
		}
	};
};

#endif


