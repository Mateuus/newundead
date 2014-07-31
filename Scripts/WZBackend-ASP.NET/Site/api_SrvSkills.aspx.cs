using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Data;
using System.Text;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Data.SqlClient;
using System.Configuration;

public partial class api_SrvSkills : WOApiWebPage
{
    void OutfitOp( string CharID,string SkillID)
    {

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_SRV_SkillAddNew";
        sqcmd.Parameters.AddWithValue("@in_CharID", CharID);
        sqcmd.Parameters.AddWithValue("@in_SkillID", SkillID);

        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");
    }

    protected override void Execute()
    {
        if (!WoCheckLoginSession())
            return;

//         string func = web.Param("func");
       string SkillID = web.Param("SkillID");
        string CharID = web.Param("CharID");

        OutfitOp(CharID,SkillID);
    }
}