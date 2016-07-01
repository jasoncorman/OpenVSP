//
// This file is released under the terms of the NASA Open Source Agreement (NOSA)
// version 1.3 as detailed in the LICENSE file which accompanies this software.
//

//////////////////////////////////////////////////////////////////////
// SubSurface.cpp
// Alex Gary
//////////////////////////////////////////////////////////////////////

#include "SubSurface.h"
#include "Geom.h"
#include "Vehicle.h"
#include "ParmMgr.h"

#include "eli/geom/intersect/specified_distance_curve.hpp"

SubSurface::SubSurface( string compID, int type )
{
    m_Type = type;
    m_CompID = compID;
    m_Tag = 0;
    m_LineColor = vec3d( 0, 0, 0 );
    m_PolyPntsReadyFlag = false;
    m_FirstSplit = true;
    m_PolyFlag = true;

    m_MainSurfIndx.Init( "MainSurfIndx", "SubSurface", this, -1, -1, 1e12 );
    m_MainSurfIndx.SetDescript( "Surface index for subsurface" );

    m_IncludeFlag.Init("IncludeFlag", "SubSurface", this, true, false, true);
    m_IncludeFlag.SetDescript("Indicates whether or not to include wetted area of subsurf in parasite drag calcs");
}

SubSurface::~SubSurface()
{
}

void SubSurface::ParmChanged( Parm* parm_ptr, int type )
{
    if ( type == Parm::SET )
    {
        m_LateUpdateFlag = true;
        return;
    }

    Update();

    Vehicle* veh = VehicleMgr.GetVehicle();
    if ( veh )
    {
        veh->ParmChanged( parm_ptr, type );
    }
}

void SubSurface::SetDisplaySuffix( int num )
{
    for ( int i = 0 ; i < ( int )m_ParmVec.size() ; i++ )
    {
        Parm* p = ParmMgr.FindParm( m_ParmVec[i] );

        if ( p )
        {
            p->SetGroupDisplaySuffix( num );
        }
    }
}

void SubSurface::LoadDrawObjs( std::vector< DrawObj* > & draw_obj_vec )
{
    m_SubSurfDO.m_LineColor = m_LineColor;

    draw_obj_vec.push_back( &m_SubSurfDO );
}

void SubSurface::LoadAllColoredDrawObjs( std::vector < DrawObj* > & draw_obj_vec )
{
    for ( int i = 0 ; i < ( int )m_DrawObjVec.size() ; i++ )
    {
        m_DrawObjVec[i].m_LineColor = vec3d( 0, 1, 0 );
        m_DrawObjVec[i].m_GeomID = ( m_ID + to_string( ( long long )i ) );
        draw_obj_vec.push_back( &m_DrawObjVec[i] );
    }
}

void SubSurface::LoadPartialColoredDrawObjs( const string & ss_id, int surf_num, std::vector < DrawObj* > & draw_obj_vec, vec3d color )
{
    Vehicle* veh = VehicleMgr.GetVehicle();
    if ( !veh )
    {
        return;
    }
    Geom* geom = veh->FindGeom( m_CompID );
    int ncopy = geom->GetSymmIndexs( m_MainSurfIndx() ).size();

    for ( int i = surf_num ; i < ( int )m_DrawObjVec.size() ; i++ )
    {
        if( m_DrawObjVec[i].m_GeomID.substr( 0, 10 ) == ss_id  )
        {
            m_DrawObjVec[i].m_LineColor = color;
            m_DrawObjVec[i].m_GeomID = ( m_ID + to_string( ( long long )i ) );
            draw_obj_vec.push_back( &m_DrawObjVec[i] );
        }
        i += ncopy-1;
    }
}

vector< TMesh* > SubSurface::CreateTMeshVec()
{
    vector<TMesh*> tmesh_vec;
    tmesh_vec.resize( m_LVec.size() );
    for ( int ls = 0 ; ls < ( int ) m_LVec.size() ; ls++ )
    {
        tmesh_vec[ls] = m_LVec[ls].CreateTMesh();
    }

    return tmesh_vec;
}

void SubSurface::UpdateDrawObjs()
{
    Vehicle* veh = VehicleMgr.GetVehicle();
    if ( !veh )
    {
        return;
    }
    Geom* geom = veh->FindGeom( m_CompID );

    m_SubSurfDO.m_PntVec.clear();
    m_SubSurfDO.m_GeomID = m_ID + string( "_ss_line" );
    m_SubSurfDO.m_LineWidth = 3.0;
    m_SubSurfDO.m_Type = DrawObj::VSP_LINES;

    if ( geom )
    {
        vector< VspSurf > surf_vec;
        geom->GetSurfVec( surf_vec );
        int ncopy = geom->GetNumSymmCopies();

        for ( int ls = 0 ; ls < ( int )m_LVec.size() ; ls++ )
        {
            int num_pnts = CompNumDrawPnts( geom );
            int *num_pnts_ptr = NULL;
            if ( num_pnts > 0 )
            {
                num_pnts_ptr = &num_pnts;
            }

            int isurf = m_MainSurfIndx();

            vector < int > symms = geom->GetSymmIndexs( isurf );
            assert( ncopy == symms.size() );

            for ( int s = 0 ; s < ncopy ; s++ )
            {
                vector < vec3d > pts;
                m_LVec[ls].GetDOPts( &surf_vec[symms[s]], geom, pts, num_pnts_ptr );
                m_SubSurfDO.m_PntVec.insert( m_SubSurfDO.m_PntVec.begin(), pts.begin(), pts.end() );
            }
        }
    }
    m_SubSurfDO.m_GeomChanged = true;
}

void SubSurface::Update()
{
    m_PolyPntsReadyFlag = false;
    UpdateDrawObjs();
}

std::string SubSurface::GetTypeName( int type )
{
    if ( type == vsp::SS_LINE )
    {
        return string( "Line" );
    }
    if ( type == vsp::SS_RECTANGLE )
    {
        return string( "Rectangle" );
    }
    if ( type == vsp::SS_ELLIPSE )
    {
        return string( "Ellipse" );
    }
    if ( type == vsp::SS_CONTROL )
    {
        return string( "Control_Surf" );
    }
    return string( "NONE" );
}


// Encode Data into XML file
xmlNodePtr SubSurface::EncodeXml( xmlNodePtr & node )
{
    ParmContainer::EncodeXml( node );

    xmlNodePtr ss_info = xmlNewChild( node, NULL, BAD_CAST "SubSurfaceInfo", NULL );
    XmlUtil::AddIntNode( ss_info, "Type", m_Type );

    return ss_info;
}

bool SubSurface::Subtag( const vec3d & center )
{
    UpdatePolygonPnts(); // Update polygon vector

    for ( int p = 0; p < ( int )m_PolyPntsVec.size(); p++ )
    {
        bool inPoly = PointInPolygon( vec2d( center.x(), center.y() ), m_PolyPntsVec[p] );

        if ( inPoly && m_TestType() == vsp::INSIDE )
        {
            return true;
        }
        else if ( inPoly && m_TestType() == vsp::OUTSIDE )
        {
            return false;
        }
    }

    if ( m_TestType() == vsp::OUTSIDE )
    {
        return true;
    }

    return false;
}

//==================================//
// This method updates the polygon points that define the polygon(s) used for the
// point in polygon test used to determine which triangles are inside or outside
// of the subsurface region
//==================================//
void SubSurface::UpdatePolygonPnts()
{
    if ( m_PolyPntsReadyFlag )
    {
        return;
    }

    m_PolyPntsVec.resize( 1 );

    m_PolyPntsVec[0].clear();

    int last_ind = m_LVec.size() - 1;
    vec3d pnt;
    for ( int ls = 0; ls < last_ind + 1; ls++ )
    {
        pnt = m_LVec[ls].GetP0();
        m_PolyPntsVec[0].push_back( vec2d( pnt.x(), pnt.y() ) );
    }
    pnt = m_LVec[last_ind].GetP1();
    m_PolyPntsVec[0].push_back( vec2d( pnt.x(), pnt.y() ) );

    m_PolyPntsReadyFlag = true;
}

bool SubSurface::Subtag( TTri* tri )
{
    vec3d center = tri->ComputeCenterUW();
    return Subtag( center );
}

void SubSurface::SplitSegsU( const double & u )
{
    double tol = 1e-10;
    int num_l_segs = m_SplitLVec.size();
    int num_splits = 0;
    bool reorder = false;
    vector<SSLineSeg> new_lsegs;
    vector<int> inds;
    for ( int i = 0 ; i < num_l_segs ; i++ )
    {
        SSLineSeg& seg = m_SplitLVec[i];
        vec3d p0 = seg.GetP0();
        vec3d p1 = seg.GetP1();

        double t = ( u - p0.x() ) / ( p1.x() - p0.x() );

        if ( t < 1 - tol && t > 0 + tol )
        {
            if ( m_FirstSplit )
            {
                m_FirstSplit = false;
                reorder = true;
            }
            // Split the segments
            vec3d int_pnt = point_on_line( p0, p1, t );
            SSLineSeg split_seg = SSLineSeg( seg );

            seg.SetP1( int_pnt );
            split_seg.SetP0( int_pnt );
            inds.push_back( i + num_splits + 1 );
            new_lsegs.push_back( split_seg );
            num_splits++;
        }
    }

    for ( int i = 0; i < ( int )inds.size() ; i++ )
    {
        m_SplitLVec.insert( m_SplitLVec.begin() + inds[i], new_lsegs[i] );
    }

    if ( reorder )
    {
        ReorderSplitSegs( inds[0] );
    }
}

void SubSurface::SplitSegsW( const double & w )
{
    double tol = 1e-10;
    int num_l_segs = m_SplitLVec.size();
    int num_splits = 0;
    bool reorder = false;
    vector<SSLineSeg> new_lsegs;
    vector<int> inds;
    for ( int i = 0 ; i < num_l_segs ; i++ )
    {

        SSLineSeg& seg = m_SplitLVec[i];
        vec3d p0 = seg.GetP0();
        vec3d p1 = seg.GetP1();

        double t = ( w - p0.y() ) / ( p1.y() - p0.y() );

        if ( t < 1 - tol && t > 0 + tol )
        {
            if ( m_FirstSplit )
            {
                m_FirstSplit = false;
                reorder = true;
            }
            // Split the segments
            vec3d int_pnt = point_on_line( p0, p1, t );
            SSLineSeg split_seg = SSLineSeg( seg );

            seg.SetP1( int_pnt );
            split_seg.SetP0( int_pnt );
            inds.push_back( i + num_splits + 1 );
            new_lsegs.push_back( split_seg );
            num_splits++;
        }
    }

    for ( int i = 0; i < ( int )inds.size() ; i++ )
    {
        m_SplitLVec.insert( m_SplitLVec.begin() + inds[i], new_lsegs[i] );
    }

    if ( reorder )
    {
        ReorderSplitSegs( inds[0] );
    }
}

void SubSurface::ReorderSplitSegs( int ind )
{
    if ( ind < 0 || ind > ( int )m_SplitLVec.size() - 1 )
    {
        return;
    }

    vector<SSLineSeg> ret_vec;
    ret_vec.resize( m_SplitLVec.size() );

    int cnt = 0;
    for ( int i = ind ; i < ( int )m_SplitLVec.size() ; i++ )
    {
        ret_vec[cnt] = m_SplitLVec[i];
        cnt++;
    }
    for ( int i = 0 ; i < ind ; i++ )
    {
        ret_vec[cnt] = m_SplitLVec[i];
        cnt++;
    }

    m_SplitLVec = ret_vec;
}

void SubSurface::PrepareSplitVec()
{
    m_SplitLVec.clear();
    m_FirstSplit = true;
    m_SplitLVec = m_LVec;
}

//////////////////////////////////////////////////////
//=================== SSLineSeg =====================//
//////////////////////////////////////////////////////

SSLineSeg::SSLineSeg()
{
    m_TestType = GT;
}

SSLineSeg::~SSLineSeg()
{
}

bool SSLineSeg::Subtag( const vec3d & center ) const
{
    // Compute cross product of line and first point to center
    vec3d v0c = center - m_P0;
    vec3d c_prod = cross( m_line, v0c );

    if ( m_TestType == GT && c_prod.z() > 0 )
    {
        return true;
    }
    if ( m_TestType == LT && c_prod.z() < 0 )
    {
        return true;
    }

    return false;
}

bool SSLineSeg::Subtag( TTri* tri ) const
{
    vec3d center = tri->ComputeCenterUW();

    return Subtag( center );
}

void SSLineSeg::Update( Geom* geom )
{
    VspSurf* surf = geom->GetSurfPtr();
    if ( !surf )
    {
        return;
    }

    double umax = surf->GetUMax();
    double wmax = surf->GetWMax();
    // Update none scaled points
    m_P0.set_xyz( m_SP0[0]*umax, m_SP0[1]*wmax, 0 );
    m_P1.set_xyz( m_SP1[0]*umax, m_SP1[1]*wmax, 0 );

    // Update line
    m_line = m_P1 - m_P0;
}

int SSLineSeg::CompNumDrawPnts( VspSurf* surf, Geom* geom )
{
    if ( !surf || !geom )
    {
        return 0;
    }
    double avg_num_secs = ( double )( surf->GetUMax() + surf->GetWMax() ) / 2.0;
    double avg_tess = ( double )( geom->m_TessU() + geom->m_TessW() ) / 2.0;

    return ( int )( ( avg_num_secs ) * ( avg_tess - 1 ) );
}

void SSLineSeg::UpdateDrawObj( VspSurf* surf, Geom* geom, DrawObj& draw_obj, const int *num_pnts_ptr )
{
    if ( !surf || !geom )
    {
        return;
    }

    int num_pnts;
    if ( num_pnts_ptr )
    {
        num_pnts = *num_pnts_ptr;
    }
    else
    {
        num_pnts = CompNumDrawPnts( surf, geom );
    }

    if ( num_pnts <=0 )
    {
        draw_obj.m_PntVec.clear();
        return;
    }

    draw_obj.m_PntVec.resize( 2 * num_pnts );


    vec3d pprev = CompPnt( surf, m_P0 );
    vec3d p = pprev;
    for ( int i = 0 ; i < num_pnts ; i ++ )
    {
        vec3d uw = ( m_P0 + m_line * ( ( double )i / ( num_pnts - 1 ) ) );
        p = CompPnt( surf, uw );
        draw_obj.m_PntVec[2*i] = pprev;
        draw_obj.m_PntVec[2*i+1] = p;
        pprev = p;
    }

    draw_obj.m_LineWidth = 3.0;
    draw_obj.m_LineColor = vec3d( 177.0 / 255, 1, 58.0 / 255 );
    draw_obj.m_Type = DrawObj::VSP_LINES;
    draw_obj.m_GeomChanged = true;
}

void SSLineSeg::GetDOPts( VspSurf* surf, Geom* geom, vector < vec3d > &pts, const int *num_pnts_ptr )
{
    int num_pnts;
    if ( num_pnts_ptr )
    {
        num_pnts = *num_pnts_ptr;
    }
    else
    {
        num_pnts = CompNumDrawPnts( surf, geom );
    }

    pts.resize( 2 * num_pnts );

    vec3d pprev = CompPnt( surf, m_P0 );
    vec3d p = pprev;
    for ( int i = 0 ; i < num_pnts ; i ++ )
    {
        vec3d uw = ( m_P0 + m_line * ( ( double )i / ( num_pnts - 1 ) ) );
        p = CompPnt( surf, uw );
        pts[2*i] = pprev;
        pts[2*i+1] = p;
        pprev = p;
    }
}

vec3d SSLineSeg::CompPnt( VspSurf* surf, vec3d uw_pnt ) const
{
    if ( !surf )
    {
        return vec3d();
    }

    double maxu = surf->GetUMax();
    double maxw = surf->GetWMax();

    if ( uw_pnt.x() < 0.0 )
    {
        uw_pnt.set_x( 0.0 );
    }
    else if ( uw_pnt.x() > maxu )
    {
        uw_pnt.set_x( maxu );
    }

    if ( uw_pnt.y() < 0.0 )
    {
        uw_pnt.set_y( 0.0 );
    }
    else if ( uw_pnt.y() > maxw )
    {
        uw_pnt.set_y( maxw );
    }

    return surf->CompPnt( uw_pnt.x(), uw_pnt.y() );
}

TMesh* SSLineSeg::CreateTMesh()
{
    int num_cut_lines = 0;
    int num_z_lines = 0;

    double tol = 1.0e-6;

    TMesh* tmesh = new TMesh();

    vec3d dc = m_line / ( num_cut_lines + 1.0 );
    vec3d dz = vec3d( 0, 0, 2.0 ) / ( num_z_lines + 1 );
    vec3d start = m_P0 + vec3d( 0, 0, -1 );

    int c, cz;

    vector< vector< vec3d > > pnt_mesh;
    pnt_mesh.resize( num_cut_lines + 2 );
    for ( int i = 0; i < ( int )pnt_mesh.size(); i++ )
    {
        pnt_mesh[i].resize( num_z_lines + 2 );
    }

    // Build plane
    for ( c = 0 ; c < num_cut_lines + 2 ; c++ )
    {
        for ( cz = 0 ; cz < num_z_lines + 2 ; cz++ )
        {
            pnt_mesh[c][cz] = start + dc * c + dz * cz;
        }
    }

    // Build triangles on that plane

    for ( c = 0 ; c < ( int )pnt_mesh.size() - 1 ; c++ )
    {
        for ( cz = 0 ; cz < ( int )pnt_mesh[c].size() - 1 ; cz ++ )
        {
            vec3d v0, v1, v2, v3, d01, d21, d20, d03, d23, norm;

            v0 = pnt_mesh[c][cz];
            v1 = pnt_mesh[c + 1][cz];
            v2 = pnt_mesh[c + 1][cz + 1];
            v3 = pnt_mesh[c][cz + 1];

            d21 = v2 - v1;
            d01 = v0 - v1;
            d20 = v2 - v0;

            if ( d21.mag() > tol && d01.mag() > tol && d20.mag() > tol )
            {
                norm = cross( d21, d01 );
                norm.normalize();
                tmesh->AddUWTri( v0, v1, v2, norm );
            }

            d03 = v0 - v3;
            d23 = v2 - v3;
            if ( d03.mag() > tol && d23.mag() > tol && d20.mag() > tol )
            {
                norm = cross( d03, d23 );
                norm.normalize();
                tmesh->AddUWTri( v0, v2, v3, norm );
            }
        }
    }
    return tmesh;
}

//////////////////////////////////////////////////////
//=================== SSLine =====================//
//////////////////////////////////////////////////////

SSLine::SSLine( string comp_id, int type ) : SubSurface( comp_id, type )
{
    m_ConstType.Init( "Const_Line_Type", "SubSurface", this, CONST_U, 0, 1 );
    m_ConstVal.Init( "Const_Line_Value", "SubSurface", this, 0.5, 0, 1 );
    m_ConstVal.SetDescript( "Either the U or V value of the line depending on what constant line type is choosen." );
    m_TestType.Init( "Test_Type", "SubSurface", this, SSLineSeg::GT, SSLineSeg::GT, SSLineSeg::LT );
    m_TestType.SetDescript( "Tag surface as being either greater than or less than const value line" );

    m_LVec.resize( 1 );
}

SSLine::~SSLine()
{
}

void SSLine::Update()
{
    // Using m_LVec[0] since SSLine should always only have one line segment
    // Update SSegLine points based on current values
    if ( m_ConstType() == CONST_U )
    {
        m_LVec[0].SetSP0( vec3d( m_ConstVal(), 1, 0 ) );
        m_LVec[0].SetSP1( vec3d( m_ConstVal(), 0, 0 ) );
    }
    else if ( m_ConstType() == CONST_W )
    {
        m_LVec[0].SetSP0( vec3d( 0, m_ConstVal(), 0 ) );
        m_LVec[0].SetSP1( vec3d( 1, m_ConstVal(), 0 ) );
    }

    m_LVec[0].m_TestType = m_TestType();
    Geom* geom = VehicleMgr.GetVehicle()->FindGeom( m_CompID );
    if ( !geom )
    {
        return;
    }
    m_LVec[0].Update( geom );

    SubSurface::Update();
}

int SSLine::CompNumDrawPnts( Geom* geom )
{
    VspSurf* surf = geom->GetSurfPtr();
    if ( !surf )
    {
        return 0;
    }

    if ( m_ConstType() == CONST_W )
    {
        return ( int )( surf->GetUMax() * ( geom->m_TessU() - 2 ) );
    }
    else if ( m_ConstType() == CONST_U )
    {
        return ( int )( surf->GetWMax() * ( geom->m_TessW() - 4 ) );
    }

    return -1;
}

bool SSLine::Subtag( TTri* tri )
{
    return m_LVec[0].Subtag( tri );
}

bool SSLine::Subtag( const vec3d & center )
{
    return m_LVec[0].Subtag( center );
}

//////////////////////////////////////////////////////
//=================== SSSquare =====================//
//////////////////////////////////////////////////////

//===== Constructor =====//
SSRectangle::SSRectangle( string comp_id, int type ) : SubSurface( comp_id, type )
{
    m_CenterU.Init( "Center_U", "SS_Rectangle", this, 0.5, 0, 1 );
    m_CenterU.SetDescript( "Defines the U location of the rectangle center" );
    m_CenterW.Init( "Center_W", "SS_Rectangle", this, 0.5, 0, 1 );
    m_CenterW.SetDescript( "Defines the W location of the rectangle center" );
    m_ULength.Init( "U_Length", "SS_Rectangle", this, .2, 0, 1 );
    m_ULength.SetDescript( "Defines length of rectangle in U direction before rotation" );
    m_WLength.Init( "W_Length", "SS_Rectangle", this, .2, 0, 1 );
    m_WLength.SetDescript( "Defines length of rectangle in W direction before rotation" );
    m_Theta.Init( "Theta", "SS_Rectangle", this, 0, -90, 90 );
    m_Theta.SetDescript( "Defines angle in degrees from U axis to rotate the rectangle" );
    m_TestType.Init( "Test_Type", "SS_Rectangle", this, vsp::INSIDE, vsp::INSIDE, vsp::OUTSIDE );
    m_TestType.SetDescript( "Determines whether or not the inside or outside of the region is tagged" );

    m_LVec.resize(4);
}

//===== Destructor =====//
SSRectangle::~SSRectangle()
{
}

void SSRectangle::Update()
{
    Geom* geom = VehicleMgr.GetVehicle()->FindGeom( m_CompID );
    if ( !geom )
    {
        return;
    }

    vec3d center;
    vector< vec3d > pntVec;

    center = vec3d( m_CenterU(), m_CenterW(), 0 );

    // Rotation Matrix
    Matrix4d transMat1, transMat2;
    Matrix4d rotMat;
    rotMat.loadIdentity();
    rotMat.rotateZ( m_Theta() );
    transMat1.loadIdentity();
    transMat1.translatef( center.x() * -1, center.y() * -1, 0 );
    transMat2.loadIdentity();
    transMat2.translatef( center.x(), center.y(), 0 );

    // Make points in counter clockwise fashion;
    pntVec.resize( 5 );
    pntVec[0] = center + vec3d( m_ULength(), m_WLength(), 0 ) * -0.5;
    pntVec[1] = center + vec3d( m_ULength(), -1.0 * m_WLength(), 0 ) * 0.5;
    pntVec[2] = center + vec3d( m_ULength(), m_WLength(), 0 ) * 0.5;
    pntVec[3] = center + vec3d( -1.0 * m_ULength(), m_WLength(), 0 ) * 0.5;
    pntVec[4] = pntVec[0];

    // Apply transformations
    for ( int i = 0; i < 5 ; i++ )
    {
        pntVec[i] = transMat2.xform( rotMat.xform( transMat1.xform( pntVec[i] ) ) );
    }

    int pind = 0;

    for ( int i = 0 ; i < 4; i++ )
    {
        m_LVec[i].SetSP0( pntVec[pind] );
        pind++;
        m_LVec[i].SetSP1( pntVec[pind] );
        m_LVec[i].Update( geom );
    }

    SubSurface::Update();
}

//////////////////////////////////////////////////////
//=================== SSEllipse =====================//
//////////////////////////////////////////////////////

//====== Constructor =====//
SSEllipse::SSEllipse( string comp_id, int type ) : SubSurface( comp_id, type )
{
    m_CenterU.Init( "Center_U", "SS_Ellipse", this, 0.5, 0, 1 );
    m_CenterU.SetDescript( "Defines the U location of the ellipse center" );
    m_CenterW.Init( "Center_W", "SS_Ellipse", this, 0.5, 0, 1 );
    m_CenterW.SetDescript( "Defines the W location of the ellipse center" );
    m_ULength.Init( "U_Length", "SS_Ellipse", this, 0.2, 0, 1 );
    m_ULength.SetDescript( "Length of ellipse in the u direction" );
    m_WLength.Init( "W_Length", "SS_Ellipse", this, 0.2, 0, 1 );
    m_WLength.SetDescript( "Length of ellipse in the w direction" );
    m_Theta.Init( "Theta", "SS_Ellipse", this, 0, -90, 90 );
    m_Theta.SetDescript( "Defines angle in degrees from U axis to rotate the rectangle" );
    m_Tess.Init( "Tess_Num", "SS_Ellipse", this, 15, 3, 1000 );
    m_Tess.SetDescript( " Number of points to discretize curve" );
    m_TestType.Init( "Test_Type", "SS_Ellipse", this, vsp::INSIDE, vsp::INSIDE, vsp::OUTSIDE );
    m_TestType.SetDescript( "Determines whether or not the inside or outside of the region is tagged" );

    m_PolyFlag = false;
}

//===== Destructor =====//
SSEllipse::~SSEllipse()
{

}

// Main Update Routine
void SSEllipse::Update()
{
    Geom* geom = VehicleMgr.GetVehicle()->FindGeom( m_CompID );
    if ( !geom )
    {
        return;
    }

    int num_pnts = m_Tess();
    m_LVec.resize( num_pnts );

    vec3d center;

    center = vec3d( m_CenterU(), m_CenterW(), 0 );

    // Rotation Matrix
    Matrix4d transMat1, transMat2;
    Matrix4d rotMat;
    rotMat.loadIdentity();
    rotMat.rotateZ( m_Theta() );
    transMat1.loadIdentity();
    transMat1.translatef( center.x() * -1, center.y() * -1, 0 );
    transMat2.loadIdentity();
    transMat2.translatef( center.x(), center.y(), 0 );

    double a = m_ULength() / 2;
    double b = m_WLength() / 2;
    for ( int i = 0 ; i < num_pnts ; i++ )
    {
        double p0 = 2 * PI * ( double )i / num_pnts;
        double p1 = 2 * PI * ( double )( i + 1 ) / num_pnts;
        vec3d pnt = vec3d();
        pnt.set_xyz( a * cos( p0 ) + m_CenterU(), b * sin( p0 ) + m_CenterW(), 0 );
        pnt = transMat2.xform( rotMat.xform( transMat1.xform( pnt ) ) );
        m_LVec[i].SetSP0( pnt );
        pnt.set_xyz( a * cos( p1 ) + m_CenterU(), b * sin( p1 ) + m_CenterW(), 0 );
        pnt = transMat2.xform( rotMat.xform( transMat1.xform( pnt ) ) );
        m_LVec[i].SetSP1( pnt );
        m_LVec[i].Update( geom );
    }

    SubSurface::Update();

}

//////////////////////////////////////////////////////
//=================== SSControlSurface =====================//
//////////////////////////////////////////////////////

SSControlSurf::SSControlSurf( string compID, int type ) : SubSurface( compID, type )
{
    m_StartLenFrac.Init( "Length_C_Start", "SS_Control", this, 0.25, 0, 1 );
    m_StartLenFrac.SetDescript( "Specifies control surface width as fraction of chord" );

    m_EndLenFrac.Init( "Length_C_End", "SS_Control", this, 0.25, 0, 1 );
    m_EndLenFrac.SetDescript( "Specifies control surface width as fraction of chord" );

    m_StartLength.Init( "Length_Start", "SS_Control", this, 1.0, 0, 1e12 );
    m_StartLength.SetDescript( "Control surface width." );

    m_EndLength.Init( "Length_End", "SS_Control", this, 1.0, 0, 1e12 );
    m_EndLength.SetDescript( "Control surface width." );

    m_AbsRelFlag.Init( "Abs_Rel_Flag", "SS_Control", this, vsp::REL, vsp::ABS, vsp::REL );
    m_AbsRelFlag.SetDescript( "Specify control surface with absolute or relative parameter." );

    m_UStart.Init( "UStart", "SS_Control", this, 0.4, 0, 1 );
    m_UStart.SetDescript( "The U starting location of the control surface" );

    m_UEnd.Init( "UEnd", "SS_Control", this, 0.6, 0, 1 );
    m_UEnd.SetDescript( "The U ending location of the control surface" );

    m_TestType.Init( "Test_Type", "SS_Control", this, vsp::INSIDE, vsp::INSIDE, vsp::OUTSIDE );
    m_TestType.SetDescript( "Determines whether or not the inside or outside of the region is tagged" );

    m_SurfType.Init( "Surf_Type", "SS_Control", this, BOTH_SURF, UPPER_SURF, BOTH_SURF );
    m_SurfType.SetDescript( "Flag to determine whether the control surface is on the upper,lower, or both surface(s) of the wing" );

    m_ConstFlag.Init( "SE_Const_Flag", "SS_Control", this, true, 0, 1 );
    m_ConstFlag.SetDescript( "Control surface start/end parameters equal." );

    m_LEFlag.Init( "LE_Flag", "SS_Control", this, false, 0, 1 );
    m_LEFlag.SetDescript( "Flag to determine whether control surface is on the leading/trailing edge." );

    m_StartAngle.Init( "StartAngle", "SS_Control", this, 90, 0, 180 );
    m_StartAngle.SetDescript( "Angle that control surface start meets leading/trailing edge." );

    m_EndAngle.Init( "EndAngle", "SS_Control", this, 90, 0, 180 );
    m_EndAngle.SetDescript( "Angle that control surface end meets leading/trailing edge." );

    m_StartAngleFlag.Init( "StartAngleFlag", "SS_Control", this, false, 0, 1 );
    m_StartAngleFlag.SetDescript( "Flag to determine whether to set control surface start angle." );

    m_EndAngleFlag.Init( "EndAngleFlag", "SS_Control", this, false, 0, 1 );
    m_EndAngleFlag.SetDescript( "Flag to determine whether to set control surface end angle." );

    m_SameAngleFlag.Init( "SameAngleFlag", "SS_Control", this, true, 0, 1 );
    m_SameAngleFlag.SetDescript( "Flag to set control surface start/end angles equal." );

    for ( int i = 0; i < 3; i++ )
    {
        m_LVec.push_back( SSLineSeg() );
    }
}

//==== Destructor ====//
SSControlSurf::~SSControlSurf()
{

}

//==== Update Method ===//
void SSControlSurf::Update()
{
    // Build Control Surface as a rectangle with the points counter clockwise

    vec3d c_uws_upper, c_uws_lower, c_uwe_upper, c_uwe_lower;
    vec3d c_uwsm_upper, c_uwsm_lower, c_uwem_upper, c_uwem_lower;
    vec3d c_uwm_upper, c_uwm_lower;

    Geom* geom = VehicleMgr.GetVehicle()->FindGeom( m_CompID );
    if ( !geom ) { return; }

    VspSurf* surf = geom->GetSurfPtr();
    if ( !surf ) { return; }

    m_UWStart.clear();
    m_UWEnd.clear();

    VspCurve startcrv;
    surf->GetU01ConstCurve( startcrv, m_UStart() );

    piecewise_curve_type c = startcrv.GetCurve();

    double vmin = c.get_parameter_min(); // Really must be 0.0
    double vmax = c.get_parameter_max(); // Really should be 4.0

    double umax = surf->GetUMax();
    double umin = 0.0;
    double ucs = m_UStart() * umax;

    double vle = ( vmin + vmax ) * 0.5;

    double vtelow = vmin + TMAGIC;
    double vteup = vmax - TMAGIC;
    double vlelow = vle - TMAGIC;
    double vleup = vle + TMAGIC;

    curve_point_type te, le;
    te = c.f( vmin );
    le = c.f( vle );

    double chord, d;
    chord = dist( le, te );

    if ( m_AbsRelFlag() == vsp::REL )
    {
        d = chord * m_StartLenFrac();
        m_StartLength.Set( d );
    }
    else
    {
        d = m_StartLength();
        m_StartLenFrac.Set( d / chord );
    }

    if ( m_ConstFlag.Get() )
    {
        if ( m_AbsRelFlag() == vsp::REL )
        {
            m_EndLenFrac.Set( d / chord );
        }
        else
        {
            m_EndLength.Set( d );
        }
    }

    if ( m_SameAngleFlag() && m_EndAngleFlag() && m_StartAngleFlag() )
    {
        m_EndAngle = m_StartAngle();
    }

    curve_point_type telow, teup;
    telow = c.f( vtelow );
    teup = c.f( vteup );

    curve_point_type lelow, leup;
    lelow = c.f( vlelow );
    leup = c.f( vleup );

    double u, v;

    if ( m_SurfType() != LOWER_SURF )
    {
        if ( !m_LEFlag() )
        {
            if ( m_StartAngleFlag() )
            {
                vec3d udir = surf->CompTanU( ucs, vteup );
                vec3d vdir;
                vdir = ( le - te ) / 2.0; // reverse direction
                double du, dv;
                surf->GuessDistanceAngle( du, dv, udir, vdir, d, m_StartAngle() * PI / 180.0 );
                udir.normalize();
                surf->FindDistanceAngle( u, v, teup, udir, d, m_StartAngle() * PI / 180.0, ucs + du, vteup - dv ); // reverse v
                c_uws_upper = vec3d( u / umax, v / vmax, 0 );

                surf->FindDistanceAngle( u, v, teup, udir, 0.5 * d, m_StartAngle() * PI / 180.0, ucs + 0.5 * du, vteup - 0.5 * dv ); // reverse v
                c_uwsm_upper = vec3d( u / umax, v / vmax, 0 );
            }
            else
            {
                piecewise_curve_type clow, cup;
                c.split( clow, cup, vle );
                eli::geom::intersect::specified_distance( v, cup, teup, d );
                c_uws_upper = vec3d( m_UStart(), v / vmax, 0 );

                eli::geom::intersect::specified_distance( v, cup, teup, 0.5 * d );
                c_uwsm_upper = vec3d( m_UStart(), v / vmax, 0 );
            }
        }
        else
        {
            if ( m_StartAngleFlag() )
            {
                vec3d udir = surf->CompTanU( ucs, vleup );
                vec3d vdir;
                vdir = ( te - le ) / 2.0;
                double du, dv;
                surf->GuessDistanceAngle( du, dv, udir, vdir, d, m_StartAngle() * PI / 180.0 );
                udir.normalize();
                surf->FindDistanceAngle( u, v, leup, udir, d, m_StartAngle() * PI / 180.0, ucs + du, vleup + dv );
                c_uws_upper = vec3d( u / umax, v / vmax, 0 );

                surf->FindDistanceAngle( u, v, leup, udir, 0.5 * d, m_StartAngle() * PI / 180.0, ucs + 0.5 * du, vleup + 0.5 * dv );
                c_uwsm_upper = vec3d( u / umax, v / vmax, 0 );
            }
            else
            {
                piecewise_curve_type clow, cup;
                c.split( clow, cup, vle );
                eli::geom::intersect::specified_distance( v, cup, leup, d );
                c_uws_upper = vec3d( m_UStart(), v / vmax, 0 );

                eli::geom::intersect::specified_distance( v, cup, leup, 0.5 * d );
                c_uwsm_upper = vec3d( m_UStart(), v / vmax, 0 );
            }
        }
        m_UWStart.push_back( c_uws_upper );
    }

    if ( m_SurfType() != UPPER_SURF )
    {
        if ( !m_LEFlag() )
        {
            if ( m_StartAngleFlag() )
            {
                vec3d udir = surf->CompTanU( ucs, vtelow );
                vec3d vdir;
                vdir = ( le - te ) / 2.0;
                double du, dv;
                surf->GuessDistanceAngle( du, dv, udir, vdir, d, m_StartAngle() * PI / 180.0 );
                udir.normalize();
                surf->FindDistanceAngle( u, v, telow, udir, d, m_StartAngle() * PI / 180.0, ucs + du, vtelow + dv );
                c_uws_lower = vec3d( u / umax, v / vmax, 0 );

                surf->FindDistanceAngle( u, v, telow, udir, 0.5 * d, m_StartAngle() * PI / 180.0, ucs + 0.5 * du, vtelow + 0.5 * dv );
                c_uwsm_lower = vec3d( u / umax, v / vmax, 0 );
            }
            else
            {
                piecewise_curve_type clow, cup;
                c.split( clow, cup, vle );
                eli::geom::intersect::specified_distance( v, clow, telow, d );
                c_uws_lower = vec3d( m_UStart(), v / vmax, 0 );

                eli::geom::intersect::specified_distance( v, clow, telow, 0.5 * d );
                c_uwsm_lower = vec3d( m_UStart(), v / vmax, 0 );
            }
        }
        else
        {
            if ( m_StartAngleFlag() )
            {
                vec3d udir = surf->CompTanU( ucs, vlelow );
                vec3d vdir;
                vdir = ( te - le ) / 2.0;
                double du, dv;
                surf->GuessDistanceAngle( du, dv, udir, vdir, d, m_StartAngle() * PI / 180.0 );
                udir.normalize();
                surf->FindDistanceAngle( u, v, lelow, udir, d, m_StartAngle() * PI / 180.0, ucs + du, vlelow - dv );
                c_uws_lower = vec3d( u / umax, v / vmax, 0 );

                surf->FindDistanceAngle( u, v, lelow, udir, 0.5 * d, m_StartAngle() * PI / 180.0, ucs + 0.5 * du, vlelow - 0.5 * dv );
                c_uwsm_lower = vec3d( u / umax, v / vmax, 0 );
            }
            else
            {
                piecewise_curve_type clow, cup;
                c.split( clow, cup, vle );
                eli::geom::intersect::specified_distance( v, clow, lelow, d );
                c_uws_lower = vec3d( m_UStart(), v / vmax, 0 );

                eli::geom::intersect::specified_distance( v, clow, lelow, 0.5 * d );
                c_uwsm_lower = vec3d( m_UStart(), v / vmax, 0 );
            }
        }
        m_UWStart.push_back( c_uws_lower );
    }

    if ( !m_StartAngleFlag() )
    {
        if ( m_LEFlag() )
        {
            vec3d udir = surf->CompTanU( ucs, vleup );
            udir.normalize();
            vec3d vdir;
            vdir = ( te - le ) / 2.0;
            vdir.normalize();
            m_StartAngle = acos( dot( udir, vdir ) ) * 180.0 / PI;
        }
        else
        {
            vec3d udir = surf->CompTanU( ucs, vtelow );
            udir.normalize();
            vec3d vdir;
            vdir = ( le - te ) / 2.0;
            vdir.normalize();
            m_StartAngle = acos( dot( udir, vdir ) ) * 180.0 / PI;
        }
    }

    VspCurve endcrv;
    surf->GetU01ConstCurve( endcrv, m_UEnd() );
    c = endcrv.GetCurve();

    te = c.f( vmin );
    le = c.f( vle );

    chord = dist( le, te );


    if ( m_AbsRelFlag() == vsp::REL )
    {
        d = chord * m_EndLenFrac();
        m_EndLength.Set( d );
    }
    else
    {
        d = m_EndLength();
        m_EndLenFrac.Set( d / chord );
    }

    telow = c.f( vtelow );
    teup = c.f( vteup );
    lelow = c.f( vlelow );
    leup = c.f( vleup );

    ucs = m_UEnd() * umax;

    if ( m_SurfType() != LOWER_SURF )
    {
        if ( !m_LEFlag() )
        {
            if ( m_EndAngleFlag() )
            {
                vec3d udir = surf->CompTanU( ucs, vteup );
                vec3d vdir;
                vdir = ( le - te ) / 2.0; // reverse direction
                double du, dv;
                surf->GuessDistanceAngle( du, dv, udir, vdir, d, m_EndAngle() * PI / 180.0 );
                udir.normalize();
                surf->FindDistanceAngle( u, v, teup, udir, d, m_EndAngle() * PI / 180.0, ucs + du, vteup - dv ); // reverse v
                c_uwe_upper = vec3d( u / umax, v / vmax, 0 );

                surf->FindDistanceAngle( u, v, teup, udir, 0.5 * d, m_EndAngle() * PI / 180.0, ucs + 0.5 * du, vteup - 0.5 * dv ); // reverse v
                c_uwem_upper = vec3d( u / umax, v / vmax, 0 );
            }
            else
            {
                piecewise_curve_type clow, cup;
                c.split( clow, cup, vle );
                eli::geom::intersect::specified_distance( v, cup, teup, d );
                c_uwe_upper = vec3d( m_UEnd(), v / vmax, 0 );

                eli::geom::intersect::specified_distance( v, cup, teup, 0.5 * d );
                c_uwem_upper = vec3d( m_UEnd(), v / vmax, 0 );
            }
        }
        else
        {
            if ( m_EndAngleFlag() )
            {
                vec3d udir = surf->CompTanU( ucs, vleup );
                vec3d vdir;
                vdir = ( te - le ) / 2.0;
                double du, dv;
                surf->GuessDistanceAngle( du, dv, udir, vdir, d, m_EndAngle() * PI / 180.0 );
                udir.normalize();
                surf->FindDistanceAngle( u, v, leup, udir, d, m_EndAngle() * PI / 180.0, ucs + du, vleup + dv );
                c_uwe_upper = vec3d( u / umax, v / vmax, 0 );

                surf->FindDistanceAngle( u, v, leup, udir, 0.5 * d, m_EndAngle() * PI / 180.0, ucs + 0.5 * du, vleup + 0.5 * dv );
                c_uwem_upper = vec3d( u / umax, v / vmax, 0 );
            }
            else
            {
                piecewise_curve_type clow, cup;
                c.split( clow, cup, vle );
                eli::geom::intersect::specified_distance( v, cup, leup, d );
                c_uwe_upper = vec3d( m_UEnd(), v / vmax, 0 );

                eli::geom::intersect::specified_distance( v, cup, leup, 0.5 * d );
                c_uwem_upper = vec3d( m_UEnd(), v / vmax, 0 );
            }
        }
        m_UWEnd.push_back( c_uwe_upper );
    }

    if ( m_SurfType() != UPPER_SURF )
    {
        if ( !m_LEFlag() )
        {
            if ( m_EndAngleFlag() )
            {
                vec3d udir = surf->CompTanU( ucs, vtelow );
                vec3d vdir;
                vdir = ( le - te ) / 2.0;
                double du, dv;
                surf->GuessDistanceAngle( du, dv, udir, vdir, d, m_EndAngle() * PI / 180.0 );
                udir.normalize();
                surf->FindDistanceAngle( u, v, telow, udir, d, m_EndAngle() * PI / 180.0, ucs + du, vtelow + dv );
                c_uwe_lower = vec3d( u / umax, v / vmax, 0 );

                surf->FindDistanceAngle( u, v, telow, udir, 0.5 * d, m_EndAngle() * PI / 180.0, ucs + 0.5 * du, vtelow + 0.5 * dv );
                c_uwem_lower = vec3d( u / umax, v / vmax, 0 );
            }
            else
            {
                piecewise_curve_type clow, cup;
                c.split( clow, cup, vle );
                eli::geom::intersect::specified_distance( v, clow, telow, d );
                c_uwe_lower = vec3d( m_UEnd(), v / vmax, 0 );

                eli::geom::intersect::specified_distance( v, clow, telow, 0.5 * d );
                c_uwem_lower = vec3d( m_UEnd(), v / vmax, 0 );
            }
        }
        else
        {
            if ( m_EndAngleFlag() )
            {
                vec3d udir = surf->CompTanU( ucs, vlelow );
                vec3d vdir;
                vdir = ( te - le ) / 2.0;
                double du, dv;
                surf->GuessDistanceAngle( du, dv, udir, vdir, d, m_EndAngle() * PI / 180.0 );
                udir.normalize();
                surf->FindDistanceAngle( u, v, lelow, udir, d, m_EndAngle() * PI / 180.0, ucs + du, vlelow - dv );
                c_uwe_lower = vec3d( u / umax, v / vmax, 0 );

                surf->FindDistanceAngle( u, v, lelow, udir, 0.5 * d, m_EndAngle() * PI / 180.0, ucs + 0.5 * du, vlelow - 0.5 * dv );
                c_uwem_lower = vec3d( u / umax, v / vmax, 0 );
            }
            else
            {
                piecewise_curve_type clow, cup;
                c.split( clow, cup, vle );
                eli::geom::intersect::specified_distance( v, clow, lelow, d );
                c_uwe_lower = vec3d( m_UEnd(), v / vmax, 0 );

                eli::geom::intersect::specified_distance( v, clow, lelow, 0.5 * d );
                c_uwem_lower = vec3d( m_UEnd(), v / vmax, 0 );
            }
        }
        m_UWEnd.push_back( c_uwe_lower );
    }

    if ( !m_EndAngleFlag() )
    {
        if ( m_LEFlag() )
        {
            vec3d udir = surf->CompTanU( ucs, vleup );
            udir.normalize();
            vec3d vdir;
            vdir = ( te - le ) / 2.0;
            vdir.normalize();
            m_EndAngle = acos( dot( udir, vdir ) ) * 180.0 / PI;
        }
        else
        {
            vec3d udir = surf->CompTanU( ucs, vtelow );
            udir.normalize();
            vec3d vdir;
            vdir = ( le - te ) / 2.0;
            vdir.normalize();
            m_EndAngle = acos( dot( udir, vdir ) ) * 180.0 / PI;
        }
    }

    // Terrible hack to place middle points.
    c_uwm_upper = ( c_uws_upper + c_uwe_upper ) * 0.5;
    c_uwm_lower = ( c_uws_lower + c_uwe_lower ) * 0.5;

    // Build Control Surface
    vector< vec3d > up_pnt_vec;
    vector< vec3d > low_pnt_vec;


    up_pnt_vec.reserve( 9 );
    low_pnt_vec.reserve( 9 );
    if ( !m_LEFlag() )
    {
        if ( m_SurfType() == UPPER_SURF )
        {
            up_pnt_vec.push_back( vec3d( m_UStart(), 1, 0 ) );
            up_pnt_vec.push_back( c_uwsm_upper );
            up_pnt_vec.push_back( c_uws_upper );
            up_pnt_vec.push_back( c_uwm_upper );
            up_pnt_vec.push_back( c_uwe_upper );
            up_pnt_vec.push_back( c_uwem_upper );
            up_pnt_vec.push_back( vec3d( m_UEnd(), 1, 0 ) );
        }
        else if ( m_SurfType() == LOWER_SURF )
        {
            low_pnt_vec.push_back( vec3d( m_UStart(), 0, 0 ) );
            low_pnt_vec.push_back( c_uwsm_lower );
            low_pnt_vec.push_back( c_uws_lower );
            low_pnt_vec.push_back( c_uwm_lower );
            low_pnt_vec.push_back( c_uwe_lower );
            low_pnt_vec.push_back( c_uwem_lower );
            low_pnt_vec.push_back( vec3d( m_UEnd(), 0, 0 ) );
        }
        else
        {
            up_pnt_vec.push_back( vec3d( m_UStart(), 1, 0 ) );
            up_pnt_vec.push_back( c_uwsm_upper );
            up_pnt_vec.push_back( c_uws_upper );
            up_pnt_vec.push_back( c_uwm_upper );
            up_pnt_vec.push_back( c_uwe_upper );
            up_pnt_vec.push_back( c_uwem_upper );
            up_pnt_vec.push_back( vec3d( m_UEnd(), 1, 0 ) );

            low_pnt_vec.push_back( vec3d( m_UEnd(), 0, 0 ) );
            low_pnt_vec.push_back( c_uwem_lower );
            low_pnt_vec.push_back( c_uwe_lower );
            low_pnt_vec.push_back( c_uwm_lower );
            low_pnt_vec.push_back( c_uws_lower );
            low_pnt_vec.push_back( c_uwsm_lower );
            low_pnt_vec.push_back( vec3d( m_UStart(), 0, 0 ) );
        }
        //  pnt_vec[3] = pnt_vec[0];
    }
    else
    {
        if ( m_SurfType() == UPPER_SURF )
        {
            up_pnt_vec.push_back( vec3d( m_UEnd(), 0.5, 0 ) );
            up_pnt_vec.push_back( c_uwem_upper );
            up_pnt_vec.push_back( c_uwe_upper );
            up_pnt_vec.push_back( c_uwm_upper );
            up_pnt_vec.push_back( c_uws_upper );
            up_pnt_vec.push_back( c_uwsm_upper );
            up_pnt_vec.push_back( vec3d( m_UStart(), 0.5, 0 ) );
            up_pnt_vec.push_back( vec3d( 0.5 * ( m_UEnd() + m_UStart() ), 0.5, 0 ) );
            up_pnt_vec.push_back( up_pnt_vec[0] );
        }
        else if ( m_SurfType() == LOWER_SURF )
        {
            low_pnt_vec.push_back( vec3d( m_UEnd(), 0.5, 0 ) );
            low_pnt_vec.push_back( c_uwem_lower );
            low_pnt_vec.push_back( c_uwe_lower );
            low_pnt_vec.push_back( c_uwm_lower );
            low_pnt_vec.push_back( c_uws_lower );
            low_pnt_vec.push_back( c_uwsm_lower );
            low_pnt_vec.push_back( vec3d( m_UStart(), 0.5, 0 ) );
            low_pnt_vec.push_back( vec3d( 0.5 * ( m_UEnd() + m_UStart() ), 0.5, 0 ) );
            low_pnt_vec.push_back( low_pnt_vec[0] );
        }
        else
        {
            up_pnt_vec.push_back( vec3d( m_UEnd(), 0.5, 0 ) );
            up_pnt_vec.push_back( c_uwem_upper );
            up_pnt_vec.push_back( c_uwe_upper );
            up_pnt_vec.push_back( c_uwm_upper );
            up_pnt_vec.push_back( c_uws_upper );
            up_pnt_vec.push_back( c_uwsm_upper );
            up_pnt_vec.push_back( vec3d( m_UStart(), 0.5, 0 ) );

            low_pnt_vec.push_back( vec3d( m_UStart(), 0.5, 0 ) );
            low_pnt_vec.push_back( c_uwsm_lower );
            low_pnt_vec.push_back( c_uws_lower );
            low_pnt_vec.push_back( c_uwm_lower );
            low_pnt_vec.push_back( c_uwe_lower );
            low_pnt_vec.push_back( c_uwem_lower );
            low_pnt_vec.push_back( vec3d( m_UEnd(), 0.5, 0 ) );
        }
        //  pnt_vec[3] = pnt_vec[0];
    }


    m_LVec.clear();

    if ( !up_pnt_vec.empty() )
    {
        for ( int i = 0; i < up_pnt_vec.size() - 1; i++ )
        {
            m_LVec.push_back( SSLineSeg() );
            m_LVec.back().SetSP0( up_pnt_vec[i] );
            m_LVec.back().SetSP1( up_pnt_vec[i+1] );
            m_LVec.back().Update( geom );
        }
    }

    m_SepIndex = m_LVec.size();

    if ( !low_pnt_vec.empty() )
    {
        for ( int i = 0; i < low_pnt_vec.size() - 1; i++ )
        {
            m_LVec.push_back( SSLineSeg() );
            m_LVec.back().SetSP0( low_pnt_vec[i] );
            m_LVec.back().SetSP1( low_pnt_vec[i+1] );
            m_LVec.back().Update( geom );
        }
    }

    SubSurface::Update();
}

void SSControlSurf::UpdateDrawObjs()
{
    SubSurface::UpdateDrawObjs();

    Vehicle* veh = VehicleMgr.GetVehicle();
    if ( !veh )
    {
        return;
    }
    Geom* geom = veh->FindGeom( m_CompID );
    if ( geom )
    {
        vector< VspSurf > surf_vec;
        geom->GetSurfVec( surf_vec );
        int ncopy = geom->GetNumSymmCopies();

        m_HingeDO.m_PntVec.clear();
        m_HingeDO.m_LineWidth = 2.0;
        m_HingeDO.m_Type = DrawObj::VSP_LINES;
        m_HingeDO.m_GeomID = m_ID + string( "_ss_hinge" );
        m_HingeDO.m_GeomChanged = true;

        m_ArrowDO.m_PntVec.clear();
        m_ArrowDO.m_Type = DrawObj::VSP_SHADED_TRIS;
        m_ArrowDO.m_GeomID = m_ID + string( "_ss_arrow" );
        m_ArrowDO.m_GeomChanged = true;

        for ( int i = 0; i < 4; i++ )
        {
            m_ArrowDO.m_MaterialInfo.Ambient[i] = 0.2f;
            m_ArrowDO.m_MaterialInfo.Diffuse[i] = 0.1f;
            m_ArrowDO.m_MaterialInfo.Specular[i] = 0.7f;
            m_ArrowDO.m_MaterialInfo.Emission[i] = 0.0f;
        }
        m_ArrowDO.m_MaterialInfo.Diffuse[3] = 0.5f;
        m_ArrowDO.m_MaterialInfo.Shininess = 5.0f;


        int isurf = m_MainSurfIndx();

        vector < int > symms = geom->GetSymmIndexs( isurf );
        assert( ncopy == symms.size() );

        int npt = m_UWStart.size();

        for ( int s = 0 ; s < ncopy ; s++ )
        {
            VspSurf* surf = &( surf_vec[ symms[ s ] ] );

            vec3d pst, pend;
            for ( int i = 0; i < npt; i++ )
            {
                pst = pst + surf->CompPnt01( m_UWStart[i].x(), m_UWStart[i].y() );
                pend = pend + surf->CompPnt01( m_UWEnd[i].x(), m_UWEnd[i].y() );
            }
            pst = pst / ( 1.0 * npt );
            pend = pend / ( 1.0 * npt );

            vec3d pmid = ( pst + pend ) * 0.5;

            vec3d dir = pend - pst;
            double len = dir.mag();
            dir.normalize();

            m_HingeDO.m_PntVec.push_back( pst );
            m_HingeDO.m_PntVec.push_back( pend );

            MakeCircleArrow( pmid, dir, 0.25, m_HingeDO, m_ArrowDO );
        }
        m_ArrowDO.m_NormVec = vector <vec3d> ( m_ArrowDO.m_PntVec.size() );
    }
}

void SSControlSurf::LoadDrawObjs( std::vector< DrawObj* > & draw_obj_vec )
{
    SubSurface::LoadDrawObjs( draw_obj_vec );

    m_HingeDO.m_LineColor = m_LineColor;
    draw_obj_vec.push_back( &m_HingeDO );
    draw_obj_vec.push_back( &m_ArrowDO );
}

void SSControlSurf::UpdatePolygonPnts()
{
    if ( m_PolyPntsReadyFlag )
    {
        return;
    }

    if ( m_SurfType() == UPPER_SURF || m_SurfType() == LOWER_SURF )
    {
        SubSurface::UpdatePolygonPnts();
        vec3d pnt = m_LVec[0].GetP0();
        m_PolyPntsVec[0].push_back( vec2d( pnt.x(), pnt.y() ) );
        return;
    }

    m_PolyPntsVec.resize( 2 );

    vec3d pnt;
    int ipp = 0;
    m_PolyPntsVec[ipp].clear();
    for ( int i = 0; i < m_SepIndex; i++ )
    {
        pnt = m_LVec[i].GetP0();
        m_PolyPntsVec[ipp].push_back( vec2d( pnt.x(), pnt.y() ) );
    }
    pnt = m_LVec[ m_SepIndex - 1 ].GetP1();
    m_PolyPntsVec[ipp].push_back( vec2d( pnt.x(), pnt.y() ) );
    m_PolyPntsVec[ipp].push_back( m_PolyPntsVec[ipp][0] );

    ipp = 1;
    m_PolyPntsVec[ipp].clear();
    for ( int i = m_SepIndex; i < m_LVec.size(); i++ )
    {
        pnt = m_LVec[i].GetP0();
        m_PolyPntsVec[ipp].push_back( vec2d( pnt.x(), pnt.y() ) );
    }
    pnt = m_LVec[ m_LVec.size() - 1 ].GetP1();
    m_PolyPntsVec[ipp].push_back( vec2d( pnt.x(), pnt.y() ) );
    m_PolyPntsVec[ipp].push_back( m_PolyPntsVec[ipp][0] );

    m_PolyPntsReadyFlag = true;
}
