/*=============================================================================
	XOpenGLTemplate.h: WeedOpenGLDrv templates.

	Revision history:
		* Created by Sebastian Kaufel.
		* TOpenGLMap tempate based on TMap template using a far superior
		  hash function.
=============================================================================*/

/*----------------------------------------------------------------------------
	TOpenGLMap.
----------------------------------------------------------------------------*/

#define OPENGLMAPBASE_INITIAL_HASHCOUNT 0x8000

inline DWORD GetOpenGLTypeHash( const QWORD A )
{
	return (DWORD)A^((DWORD)(A>>16))^((DWORD)(A>>32));
}

//
// Maps unique keys to values.
//
template< class TK, class TI > class TOpenGLMapBase
{
protected:
	class TPair
	{
	public:
		INT HashNext;
		TK Key;
		TI Value;
		TPair( const TK& InKey, const TI& InValue )
		: Key( InKey ), Value( InValue )
		{}
		TPair()
		{}
		friend FArchive& operator<<( FArchive& Ar, TPair& F )
		{
			guardSlow(TOpenGLMapBase::TPair<<);
			return Ar << F.Key << F.Value;
			unguardSlow;
		}
	};
	void Rehash()
	{
		guardSlow(TOpenGLMapBase::Rehash);
		checkSlow(!(HashCount&(HashCount-1)));
		checkSlow(HashCount>=8);
		INT* NewHash = new INT[HashCount];
		{for( INT i=0; i<HashCount; i++ )
		{
			NewHash[i] = INDEX_NONE;
		}}
		{for( INT i=0; i<Pairs.Num(); i++ )
		{
			TPair& Pair    = Pairs(i);
			INT    iHash   = (GetOpenGLTypeHash(Pair.Key) & (HashCount-1));
			Pair.HashNext  = NewHash[iHash];
			NewHash[iHash] = i;
		}}
		if( Hash )
			delete[] Hash;
		Hash = NewHash;
		unguardSlow;
	}
	void Relax()
	{
		guardSlow(TOpenGLMapBase::Relax);
		while( HashCount>Pairs.Num()*2+OPENGLMAPBASE_INITIAL_HASHCOUNT )
			HashCount /= 2;
		Rehash();
		unguardSlow;
	}
	TI& Add( const TK& InKey, const TI& InValue )
	{
		guardSlow(TOpenGLMapBase::Add);
		TPair& Pair   = *new(Pairs)TPair( InKey, InValue );
		INT    iHash  = (GetOpenGLTypeHash(Pair.Key) & (HashCount-1));
		Pair.HashNext = Hash[iHash];
		Hash[iHash]   = Pairs.Num()-1;
		if( HashCount*2+1024 < Pairs.Num() )
		{
			HashCount *= 2;
			Rehash();
		}
		return Pair.Value;
		unguardSlow;
	}
	TArray<TPair> Pairs;
	INT* Hash=0;
	INT HashCount=0;
public:
	TOpenGLMapBase()
	:	Hash( NULL )
	,	HashCount( OPENGLMAPBASE_INITIAL_HASHCOUNT )
	{
		guardSlow(TOpenGLMapBase::TOpenGLMapBase);
		Rehash();
		unguardSlow;
	}
	TOpenGLMapBase( const TOpenGLMapBase& Other )
	:	Pairs( Other.Pairs )
	,	HashCount( Other.HashCount )
	,	Hash( NULL )
	{
		guardSlow(TOpenGLMapBase::TOpenGLMapBase copy);
		Rehash();
		unguardSlow;
	}
	~TOpenGLMapBase()
	{
		guardSlow(TOpenGLMapBase::~TOpenGLMapBase);
		if( Hash )
			delete[] Hash;
		Hash = NULL;
		HashCount = 0;
		unguardSlow;
	}
	TOpenGLMapBase& operator=( const TOpenGLMapBase& Other )
	{
		guardSlow(TOpenGLMapBase::operator=);
		Pairs     = Other.Pairs;
		HashCount = Other.HashCount;
		Rehash();
		return *this;
		unguardSlow;
	}
	void Empty()
	{
		guardSlow(TOpenGLMapBase::Empty);
		checkSlow(!(HashCount&(HashCount-1)));
		Pairs.Empty();
		HashCount = OPENGLMAPBASE_INITIAL_HASHCOUNT;
		Rehash();
		unguardSlow;
	}
	TI& Set( const TK& InKey, const TI& InValue )
	{
		guardSlow(TMap::Set);
		for( INT i=Hash[(GetOpenGLTypeHash(InKey) & (HashCount-1))]; i!=INDEX_NONE; i=Pairs(i).HashNext )
			if( Pairs(i).Key==InKey )
				{Pairs(i).Value=InValue; return Pairs(i).Value;}
		return Add( InKey, InValue );
		unguardSlow;
	}
	INT Remove( const TK& InKey )
	{
		guardSlow(TOpenGLMapBase::Remove);
		INT Count=0;
		for( INT i=Pairs.Num()-1; i>=0; i-- )
			if( Pairs(i).Key==InKey )
				{Pairs.Remove(i); Count++;}
		if( Count )
			Relax();
		return Count;
		unguardSlow;
	}
	TI* Find( const TK& Key )
	{
		guardSlow(TOpenGLMapBase::Find);
		for( INT i=Hash[(GetOpenGLTypeHash(Key) & (HashCount-1))]; i!=INDEX_NONE; i=Pairs(i).HashNext )
			if( Pairs(i).Key==Key )
				return &Pairs(i).Value;
		return NULL;
		unguardSlow;
	}
	TI FindRef( const TK& Key )
	{
		guardSlow(TOpenGLMapBase::Find);
		for( INT i=Hash[(GetOpenGLTypeHash(Key) & (HashCount-1))]; i!=INDEX_NONE; i=Pairs(i).HashNext )
			if( Pairs(i).Key==Key )
				return Pairs(i).Value;
		return NULL;
		unguardSlow;
	}
	const TI* Find( const TK& Key ) const
	{
		guardSlow(TOpenGLMapBase::Find);
		for( INT i=Hash[(GetOpenGLTypeHash(Key) & (HashCount-1))]; i!=INDEX_NONE; i=Pairs(i).HashNext )
			if( Pairs(i).Key==Key )
				return &Pairs(i).Value;
		return NULL;
		unguardSlow;
	}
	friend FArchive& operator<<( FArchive& Ar, TOpenGLMapBase& M )
	{
		guardSlow(TOpenGLMapBase<<);
		Ar << M.Pairs;
		if( Ar.IsLoading() )
			M.Rehash();
		return Ar;
		unguardSlow;
	}
	void Dump( FOutputDevice& Ar )
	{
		guard(TOpenGLMapBase::Dump);
		INT NonEmpty = 0, Worst = 0;
		for( INT i=0; i<HashCount; i++ )
		{
			INT c=0;
			for( INT j=Hash[i]; j!=INDEX_NONE; j=Pairs(j).HashNext )
				c++;
			if ( c>Worst )
				Worst = c;
			if ( c>0 )
			{
				NonEmpty++;
				Ar.Logf( TEXT("   Hash[%i] = %i"), i, c );
			}
		}
		Ar.Logf( TEXT("TOpenGLMapBase: %i items, worst %i, %i/%i hash slots used."), Pairs.Num(), Worst, NonEmpty, HashCount );
		unguard;
	}
	int Num()
	{
		guardSlow(TOpenGLMapBase::Num);
		return this->Pairs.Num();
		unguardSlow;
	}
	class TIterator
	{
	public:
		TIterator( TOpenGLMapBase& InMap ) : Pairs( InMap.Pairs ), Index( 0 ) {}
		void operator++()          { ++Index; }
		void RemoveCurrent()       { Pairs.Remove(Index--); }
		operator UBOOL() const     { return Index<Pairs.Num(); }
		TK& Key() const            { return Pairs(Index).Key; }
		TI& Value() const          { return Pairs(Index).Value; }
	private:
		TArray<TPair>& Pairs;
		INT Index;
	};
	friend class TIterator;
};
template< class TK, class TI > class TOpenGLMap : public TOpenGLMapBase<TK,TI>
{
public:
	TOpenGLMap& operator=( const TOpenGLMap& Other )
	{
		TOpenGLMapBase<TK,TI>::operator=( Other );
		return *this;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
