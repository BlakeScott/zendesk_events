<?php
/**
 * Implements hook_drush_command().
 */
function zendesk_events_drush_command(){
     $items['zd-command'] = array(
      'description' => 'Demonstrate how Drush commands work.',
      'aliases' => array('dzc'),
      'drush zd-command' => 'Triggers the internal note and tags for ZenDesk'
    );
   return $items;
}

function zendesk_events_fetch(){
     $items['zd-eventsfetch'] = array(
      'description' => 'Fetches Zendesk Events.',
      'aliases' => array('zdf'),
      'drush zd-eventsfetch' => 'Triggers the internal note and tags for ZenDesk'
    );
   return $items;
}
