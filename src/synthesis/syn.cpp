#include "syn.h"

syn::syn(Cudd* m, string filename, string partfile)
{
    //ctor

    //Cudd *p = &mgr;
    bdd = new DFA(m);
    bdd->initialize(filename, partfile);
    mgr = m;
    initializer();

    //bdd->bdd2dot();

}

syn::syn(Cudd* m, DFA* d)
{
    bdd = d;
    mgr = m;
    initializer();

    bdd->bdd2dot();
}

syn::~syn()
{
    //dtor
}

void syn::initializer(){
  for(int i = 0; i < bdd->nbits; i++){
    BDD b = mgr->bddVar();
    bdd->bddvars.push_back(b);
  }
    W.push_back(bdd->finalstatesBDD);
    Wprime.push_back(bdd->finalstatesBDD);
    cur = 0;


}

BDD syn::state2bdd(int s){
    string bin = state2bin(s);
    BDD b = mgr->bddOne();
    int nzero = bdd->nbits - bin.length();
    //cout<<nzero<<endl;
    for(int i = 0; i < nzero; i++){
        b *= !bdd->bddvars[i];
    }
    for(int i = 0; i < bin.length(); i++){
        if(bin[i] == '0')
            b *= !bdd->bddvars[i+nzero];
        else
            b *= bdd->bddvars[i+nzero];
    }
    return b;

}

string syn::state2bin(int n){
    string res;
    while (n)
    {
        res.push_back((n & 1) + '0');
        n >>= 1;
    }

    if (res.empty())
        res = "0";
    else
        reverse(res.begin(), res.end());
   return res;
}

bool syn::fixpoint(){
    if(W[cur] == W[cur-1])
        return true;
}

void syn::printBDDSat(BDD b){

  std::cout<<"sat with: ";
  int max = bdd->nstates;
  
  for (int i=0; i<max; i++){
    if (b.Eval(state2bit(i)).IsOne()){
      std::cout<<i<<", ";
    }
  }
  std::cout<<std::endl;
}

bool syn::realizablity_sys(unordered_map<unsigned int, BDD>& IFstrategy){
    while(true){
        BDD I = mgr->bddOne();
        int index;
        // input should be provided by environment
        for(int i = 0; i < bdd->input.size(); i++){
            // map to bdd var index
            index = bdd->input[i];
            I *= bdd->bddvars[index];
        }
        //
        BDD tmp = W[cur] + univsyn_sys(I);
        W.push_back(tmp);
        cur++;

        BDD O = mgr->bddOne();
        for(int i = 0; i < bdd->output.size(); i++){
            index = bdd->output[i];
            O *= bdd->bddvars[index];
        }

	    Wprime.push_back(existsyn_sys(O));
        if(fixpoint())
            break;
    }
    if(Wprime[cur-1].Eval(bdd->initbv).IsOne()){
        BDD O = mgr->bddOne();
        for(int i = 0; i < bdd->output.size(); i++){
            O *= bdd->bddvars[bdd->output[i]];
        }
	/*
        O *= bdd->bddvars[bdd->nbits];
        //naive synthesis
        BDD cons = W[cur].SolveEqn(O, S2O, &yindex, bdd->output.size());
        strategy(S2O);
	*/
	
        InputFirstSynthesis IFsyn(*mgr);
        IFstrategy = IFsyn.synthesize(W[cur], O);

        return true;
    }
    //std::cout<<"unrealizable, winning set: "<<std::endl;
    //std::cout<<Wprime[Wprime.size()-1]<<std::endl;
    // assert(false);
    return false;
}

bool syn::realizablity_env(std::unordered_map<unsigned, BDD>& IFstrategy){
    BDD transducer;
    while(true){
        int index;
        BDD O = mgr->bddOne();
        for(int i = 0; i < bdd->output.size(); i++){
            index = bdd->output[i];
            O *= bdd->bddvars[index];
        }

        BDD tmp = W[cur] + existsyn_env(O, transducer);
        W.push_back(tmp);
        cur++;
        if(fixpoint())
            break;

        BDD I = mgr->bddOne();
        for(int i = 0; i < bdd->input.size(); i++){
            index = bdd->input[i];
            I *= bdd->bddvars[index];
        }

        Wprime.push_back(univsyn_env(I));
        if((Wprime[cur].Eval(bdd->initbv)).IsOne()){
            return true;
        }

    }
    if((Wprime[cur-1].Eval(bdd->initbv)).IsOne()){
      // TODO: use ifstrategysynthesis
        BDD O = mgr->bddOne();
	    //vector<BDD> S2O;
        for(int i = 0; i < bdd->output.size(); i++){
            O *= bdd->bddvars[bdd->output[i]];
        }
        O *= bdd->bddvars[bdd->nbits];

	InputFirstSynthesis IFsyn(*mgr);
        IFstrategy = IFsyn.synthesize(W[cur], O);
        //naive synthesis
        //transducer.SolveEqn(O, S2O, outindex(), bdd->output.size());
        //strategy(S2O);

        return true;
    }
    return false;

}


void syn::strategy(vector<BDD>& S2O){
    vector<BDD> winning;
    for(int i = 0; i < S2O.size(); i++){
        //dumpdot(S2O[i], "S2O"+to_string(i));
        for(int j = 0; j < bdd->output.size(); j++){
            int index = bdd->output[j];
            S2O[i] = S2O[i].Compose(bdd->bddvars[index], mgr->bddOne());
        }
    }
}

int** syn::outindex(){
    int outlength = bdd->output.size();
    int outwidth = 2;
    int **out = 0;
    out = new int*[outlength];
    for(int l = 0; l < outlength; l++){
        out[l] = new int[outwidth];
        out[l][0] = l;
        out[l][1] = bdd->output[l];
    }
    return out;
}

int* syn::state2bit(int n){
    int* s = new int[bdd->nbits];
    for (int i=bdd->nbits-1; i>=0; i--){
      s[i] = n%2;
      n = n/2;
    }
    return s;
}

// needs to satisfy all possible values of input variables
BDD syn::univsyn_sys(BDD univ){

    BDD tmp = Wprime[cur];
    int offset = bdd->nbits + bdd->nvars;
    // previous/next image
    tmp = prime(tmp);
    for(int i = 0; i < bdd->nbits; i++){
        tmp = tmp.Compose(bdd->res[i], offset+i);
    }

    tmp *= !Wprime[cur];
    // abstract all input variables
    BDD eliminput = tmp.UnivAbstract(univ);
    return eliminput;

}

BDD syn::existsyn_env(BDD exist, BDD& transducer){
    BDD tmp = Wprime[cur];
    int offset = bdd->nbits + bdd->nvars;

    //dumpdot(I, "W00");
    tmp = prime(tmp);
    for(int i = 0; i < bdd->nbits; i++){
        tmp = tmp.Compose(bdd->res[i], offset+i);
    }
    transducer = tmp;
    tmp *= !Wprime[cur];
    BDD elimoutput = tmp.ExistAbstract(exist);
    return elimoutput;

}

BDD syn::univsyn_env(BDD univ){

    BDD tmp = W[cur];
    BDD elimuniv = tmp.UnivAbstract(univ);
    return elimuniv;

}

BDD syn::prime(BDD orign){
    int offset = bdd->nbits + bdd->nvars;
    BDD tmp = orign;
    for(int i = 0; i < bdd->nbits; i++){
        // Compose(F, v, G) : F(v, x) -> F(G(x), x), means substitute v = G(x)
        tmp = tmp.Compose(bdd->bddvars[i+offset], i);
    }
    return tmp;
}

BDD syn::existsyn_sys(BDD exist){

    BDD tmp = W[cur];
    BDD elimoutput = tmp.ExistAbstract(exist);
    return elimoutput;

}

void syn::dumpdot(BDD &b, string filename){
    FILE *fp = fopen(filename.c_str(), "w");
    vector<BDD> single(1);
    single[0] = b;
	this->mgr->DumpDot(single, NULL, NULL, fp);
	fclose(fp);
}


















