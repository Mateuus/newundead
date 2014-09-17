using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Data;
using System.Text;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Data.SqlClient;
using System.IO;

public partial class api_GetInventoryData : WOApiWebPage
{

    protected override void Execute()
    {
        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_GetInventoryData";

        int Data = Convert.ToInt32(web.Param("Data"));

        sqcmd.Parameters.AddWithValue("@in_Data", Data);
        sqcmd.Parameters.AddWithValue("@in_Inventory", Convert.ToInt32(web.Param("Inventory")));
        sqcmd.Parameters.AddWithValue("@in_CustomerID", Convert.ToInt64(web.Param("CustomerID")));
        sqcmd.Parameters.AddWithValue("@in_CharID", Convert.ToInt32(web.Param("CharID")));
        sqcmd.Parameters.AddWithValue("@in_Slot", Convert.ToInt32(web.Param("Slot")));
        sqcmd.Parameters.AddWithValue("@in_ItemID", Convert.ToInt32(web.Param("ItemID")));
        sqcmd.Parameters.AddWithValue("@in_Quantity", Convert.ToInt32(web.Param("Quantity")));
        sqcmd.Parameters.AddWithValue("@in_Var1", Convert.ToInt32(web.Param("Var1")));
        sqcmd.Parameters.AddWithValue("@in_Var2", Convert.ToInt32(web.Param("Var2")));
        sqcmd.Parameters.AddWithValue("@in_Category", Convert.ToInt32(web.Param("Category")));
      //  sqcmd.Parameters.AddWithValue("@in_Durability", Convert.ToInt32(web.Param("Durability")));

        if (!CallWOApi(sqcmd))
            return;

        if (Data == 0)
        {

            StringBuilder xml = new StringBuilder();
            xml.Append("<?xml version=\"1.0\"?>\n");

            while (reader.Read())
            {
                string InventoryID_2 = reader["InventoryID"].ToString();
                string CustomerID_2 = reader["CustomerID"].ToString();
                string CharID_2 = reader["CharID"].ToString();
                string Slot_2 = reader["BackpackSlot"].ToString();
                string ItemID_2 = reader["ItemID"].ToString();
                string Quantity_2 = reader["Quantity"].ToString();
                string Var1_2 = reader["Var1"].ToString();
                string Var2_2 = reader["Var2"].ToString();
               // string Durability_1 = reader["Durability"].ToString();

                xml.Append("<UsersInventory ");
                xml.Append(xml_attr("InventoryID", InventoryID_2));
                xml.Append(xml_attr("CustomerID", CustomerID_2));
                xml.Append(xml_attr("CharID", CharID_2));
                xml.Append(xml_attr("BackpackSlot", Slot_2));
                xml.Append(xml_attr("ItemID", ItemID_2));
                xml.Append(xml_attr("Quantity", Quantity_2));
                xml.Append(xml_attr("Var1", Var1_2));
                xml.Append(xml_attr("Var2", Var2_2));
             //   xml.Append(xml_attr("Durability", Durability_1));
                xml.Append("/>\n");
            }

            GResponse.Write(xml.ToString());
        }
        else
        {
            Response.Write("WO_0");
        }
    }  
}
